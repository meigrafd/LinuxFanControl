// All comments in English by request.

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <nlohmann/json.hpp>
#include <iostream>
#include <vector>
#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <algorithm>
#include <sensors/sensors.h>
#include <cstdio>

#include "hwmon.hpp"

using json = nlohmann::json;
static const char* kSockPath = "/tmp/lfcd.sock"; // for system use you may want /run/lfcd.sock

static std::string gen_id(){
  static std::atomic<unsigned long long> ctr{1};
  char buf[32]; std::snprintf(buf,sizeof(buf), "ch-%llu", (unsigned long long)ctr.fetch_add(1));
  return buf;
}

static int create_server(){
  ::unlink(kSockPath);
  int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
  if(fd<0){ perror("socket"); return -1; }
  sockaddr_un addr{}; addr.sun_family = AF_UNIX;
  std::snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", kSockPath);
  if(::bind(fd, (sockaddr*)&addr, sizeof(addr))<0){ perror("bind"); ::close(fd); return -1; }
  if(::listen(fd, 16)<0){ perror("listen"); ::close(fd); return -1; }
  return fd;
}

static bool send_json(int cfd, const json& j){
  std::string s = j.dump(); s.push_back('\n');
  ssize_t n = ::send(cfd, s.data(), s.size(), 0);
  return n==(ssize_t)s.size();
}

static bool recv_line(int cfd, std::string &out){
  out.clear(); char ch;
  while(true){
    ssize_t n = ::recv(cfd, &ch, 1, 0);
    if(n<=0) return false;
    if(ch=='\n') return true;
    out.push_back(ch);
    if(out.size()>65536) return false;
  }
}

// ------------ Engine state ------------
static std::vector<TempSensor> g_temps;
static std::vector<PwmDevice>  g_pwms;
static std::vector<Channel>    g_channels;
static std::mutex              g_mtx;
static std::atomic<bool>       g_run{false};
static std::condition_variable g_cv;

static const PwmDevice* find_pwm_by_path(const std::string& pwm_path){
  for(auto &p: g_pwms) if(p.pwm_path==pwm_path) return &p; return nullptr;
}
static Channel* find_channel_by_id(const std::string& id){
  for(auto &c: g_channels) if(c.id==id) return &c; return nullptr;
}

static void engine_thread(){
  using namespace std::chrono;
  while(true){
    std::unique_lock<std::mutex> lk(g_mtx);
    g_cv.wait_for(lk, 1s, []{return g_run.load();});
    if(!g_run.load()) continue;
    auto chans = g_channels; // snapshot without holding the lock
    lk.unlock();

    double now_s = duration_cast<duration<double>>(steady_clock::now().time_since_epoch()).count();
    for(auto &ch : chans){
      double out_pct = ch.last_out;
      try{
        if(ch.mode=="Manual"){
          out_pct = std::max(0.0, std::min(100.0, ch.manual_pct));
        }else{
          auto t = read_temp_c(ch.sensor_path);
          ch.last_temp = t;
          out_pct = ch.filter.step(ch.curve, t, now_s);
        }
        if(!ch.enable_path.has_value() || ch.enable_path->empty()==false) {
          if(ch.enable_path) set_pwm_enable(*ch.enable_path, 1);
          write_pwm_pct(ch.pwm_path, out_pct);
        }
      }catch(...){}
      std::lock_guard<std::mutex> lk2(g_mtx);
      if(auto *pch = find_channel_by_id(ch.id)) pch->last_out = out_pct;
    }
  }
}

int main(){
  sensors_init(NULL); // enable libsensors for ls://
  int sfd = create_server();
  if(sfd<0) return 1;
  std::cout<<"lfcd ready at "<<kSockPath<<"\n";

  g_temps = enumerate_temps();
  g_pwms  = enumerate_pwms();
  for(auto &p: g_pwms){ std::string reason; probe_pwm_writable(p, reason); }

  std::thread th(engine_thread); th.detach();

  while(true){
    int cfd = ::accept(sfd, nullptr, nullptr);
    if(cfd<0){ perror("accept"); continue; }
    std::string line;
    while(recv_line(cfd, line)){
      json req; try{ req = nlohmann::json::parse(line); } catch(...){ break; }
      json rsp; rsp["id"] = req.value("id", 0);
      std::string m = req.value("method", "");
      try{
        if(m=="getVersion"){
          rsp["result"] = json{{"name","lfcd"},{"version","0.1.0"}};

        }else if(m=="enumerate"){
          json jtemps = json::array();
          for(auto &t: g_temps) jtemps.push_back({{"label",t.label},{"path",t.path},{"type",t.type},{"unit",t.unit}});
          json jpwms  = json::array();
          for(auto &p: g_pwms)  jpwms.push_back({{"label",p.label},{"pwm_path",p.pwm_path},{"enable_path",p.enable_path.value_or("")},{"tach_path",p.tach_path.value_or("")},{"writable",p.writable}});
          rsp["result"] = json{{"temps", jtemps}, {"pwms", jpwms}};

        }else if(m=="readTemp"){
          auto path = req["params"].value("path", std::string());
          rsp["result"] = read_temp_c(path);

        }else if(m=="probePwm"){
          auto label = req["params"].value("label", std::string());
          auto it = std::find_if(g_pwms.begin(), g_pwms.end(), [&](auto&x){return x.label==label;});
          if(it==g_pwms.end()) throw std::runtime_error("not found");
          std::string reason; bool ok = probe_pwm_writable(*it, reason);
          rsp["result"] = json{{"ok",ok},{"reason",reason},{"writable",it->writable}};

        }else if(m=="writePwm"){
          auto path = req["params"].value("pwm_path", std::string());
          double pct = req["params"].value("pct", 0.0);
          bool ok = write_pwm_pct(path, pct);
          rsp["result"] = json{{"ok",ok}};

        }else if(m=="setEnable"){
          auto path = req["params"].value("enable_path", std::string());
          int mode = req["params"].value("mode", 1);
          rsp["result"] = json{{"ok", set_pwm_enable(path, mode)}};

          // ------- Engine & Channels -------
        }else if(m=="engineStart"){
          g_run.store(true); g_cv.notify_all(); rsp["result"]=json{{"ok",true}};

        }else if(m=="engineStop"){
          g_run.store(false); g_cv.notify_all(); rsp["result"]=json{{"ok",true}};

        }else if(m=="listChannels"){
          std::lock_guard<std::mutex> lk(g_mtx);
          json arr=json::array();
          for(auto &c: g_channels){
            json j;
            j["id"]=c.id; j["name"]=c.name; j["sensor_path"]=c.sensor_path; j["pwm_path"]=c.pwm_path;
            j["enable_path"]=c.enable_path.value_or("");
            j["mode"]=c.mode; j["manual_pct"]=c.manual_pct;
            j["curve"]=json::array(); for(auto &p: c.curve.pts) j["curve"].push_back({p.first,p.second});
            j["hyst"]=c.filter.hyst_c; j["tau"]=c.filter.tau_s;
            j["last_temp"]=c.last_temp; j["last_out"]=c.last_out;
            arr.push_back(j);
          }
          rsp["result"]=arr;

        }else if(m=="createChannel"){
          Channel c;
          c.id=gen_id();
          c.name=req["params"].value("name", std::string("Channel"));
          c.sensor_path=req["params"].value("sensor_path", std::string());
          c.pwm_path=req["params"].value("pwm_path", std::string());
          std::string ep=req["params"].value("enable_path", std::string());
          if(!ep.empty()) c.enable_path=ep;
          c.mode=req["params"].value("mode", std::string("Auto"));
          c.manual_pct=req["params"].value("manual_pct", 0.0);
          c.filter.hyst_c=req["params"].value("hyst", 0.5);
          c.filter.tau_s=req["params"].value("tau", 2.0);
          c.curve.pts.clear();
          for(auto &pt : req["params"]["curve"]) c.curve.pts.push_back({pt[0].get<double>(), pt[1].get<double>()});
          std::lock_guard<std::mutex> lk(g_mtx);
          g_channels.push_back(std::move(c));
          rsp["result"]=json{{"ok",true}};

        }else if(m=="setChannelMode"){
          auto id=req["params"].value("id", std::string());
          auto mode=req["params"].value("mode", std::string("Auto"));
          std::lock_guard<std::mutex> lk(g_mtx);
          auto *c=find_channel_by_id(id); if(!c) throw std::runtime_error("not found");
          c->mode=mode; rsp["result"]=json{{"ok",true}};

        }else if(m=="setChannelManual"){
          auto id=req["params"].value("id", std::string());
          double pct=req["params"].value("pct", 0.0);
          std::lock_guard<std::mutex> lk(g_mtx);
          auto *c=find_channel_by_id(id); if(!c) throw std::runtime_error("not found");
          c->manual_pct=pct; rsp["result"]=json{{"ok",true}};

        }else if(m=="setChannelCurve"){
          auto id=req["params"].value("id", std::string());
          std::vector<std::pair<double,double>> pts;
          for(auto &pt : req["params"]["curve"]) pts.push_back({pt[0].get<double>(), pt[1].get<double>()});
          std::lock_guard<std::mutex> lk(g_mtx);
          auto *c=find_channel_by_id(id); if(!c) throw std::runtime_error("not found");
          c->curve.pts=std::move(pts); rsp["result"]=json{{"ok",true}};

        }else if(m=="setChannelHystTau"){
          auto id=req["params"].value("id", std::string());
          double hyst=req["params"].value("hyst", 0.5), tau=req["params"].value("tau",2.0);
          std::lock_guard<std::mutex> lk(g_mtx);
          auto *c=find_channel_by_id(id); if(!c) throw std::runtime_error("not found");
          c->filter.hyst_c=hyst; c->filter.tau_s=tau; rsp["result"]=json{{"ok",true}};

        }else if(m=="deleteChannel"){
          auto id=req["params"].value("id", std::string());
          std::lock_guard<std::mutex> lk(g_mtx);
          g_channels.erase(std::remove_if(g_channels.begin(), g_channels.end(),
                                          [&](auto&x){return x.id==id;}), g_channels.end());
          rsp["result"]=json{{"ok",true}};

        }else if(m=="calibratePwm"){
          std::string pwm_label=req["params"].value("label", std::string());
          std::string pwm_path=req["params"].value("pwm_path", std::string());
          const PwmDevice* dev=nullptr;
          if(!pwm_label.empty()){
            auto it=std::find_if(g_pwms.begin(), g_pwms.end(), [&](auto&x){return x.label==pwm_label;});
            if(it!=g_pwms.end()) dev=&*it;
          }else if(!pwm_path.empty()){
            dev=find_pwm_by_path(pwm_path);
          }
          if(!dev) throw std::runtime_error("pwm not found");
          auto r = calibrate_pwm(*dev,
                                 req["params"].value("rpm_threshold", 100),
                                 req["params"].value("floor_pct", 20),
                                 req["params"].value("step", 5),
                                 req["params"].value("settle_s", 1.0),
                                 req["params"].value("boost_hold_s", 10.0));
          if(!r.ok) rsp["error"]=r.error;
          else rsp["result"]=json{{"ok",true},{"min_pct",r.min_pct},{"spinup_pct",r.spinup_pct}};

        }else if(m=="detectCoupling"){
          auto hold = req["params"].value("hold_s", 10.0);
          auto min_d= req["params"].value("min_delta_c", 1.0);
          auto tach_d=req["params"].value("rpm_delta_threshold", 80);
          auto vec = detect_coupling(g_pwms, g_temps, hold, min_d, tach_d);
          json m=json::object();
          for(auto &pr : vec){
            m[pr.first] = json{{"sensor_path",pr.second.sensor_path},{"sensor_label",pr.second.sensor_label},{"score",pr.second.score}};
          }
          rsp["result"]=m;

        }else{
          throw std::runtime_error("unknown method: "+m);
        }
      }catch(const std::exception& e){
        rsp["error"] = e.what();
      }
      if(!send_json(cfd, rsp)) break;
    }
    ::close(cfd);
  }
  return 0;
}
