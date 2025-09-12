#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <nlohmann/json.hpp>
#include <iostream>
#include <vector>
#include <string>
#include "hwmon.hpp"

using json = nlohmann::json;
static const char* kSockPath = "/tmp/lfcd.sock"; // non-root; für Systembetrieb z.B. /run/lfcd.sock

static int create_server(){
  ::unlink(kSockPath);
  int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
  if(fd<0){ perror("socket"); return -1; }
  sockaddr_un addr{}; addr.sun_family = AF_UNIX;
  std::snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", kSockPath);
  if(::bind(fd, (sockaddr*)&addr, sizeof(addr))<0){ perror("bind"); ::close(fd); return -1; }
  if(::listen(fd, 8)<0){ perror("listen"); ::close(fd); return -1; }
  return fd;
}

static bool send_json(int cfd, const json& j){
  auto s = j.dump();
  s.push_back('\n');
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

int main(){
  int sfd = create_server();
  if(sfd<0) return 1;
  std::cout<<"lfcd ready at "<<kSockPath<<"\n";
  auto temps = enumerate_temps();
  auto pwms  = enumerate_pwms();
  for(auto &p: pwms){ std::string reason; probe_pwm_writable(p, reason); }

  while(true){
    int cfd = ::accept(sfd, nullptr, nullptr);
    if(cfd<0){ perror("accept"); continue; }
    std::string line;
    while(recv_line(cfd, line)){
      json req; try{ req = json::parse(line); } catch(...){ break; }
      json rsp; rsp["id"] = req.value("id", 0);
      std::string m = req.value("method", "");
      try{
        if(m=="getVersion"){
          rsp["result"] = json{{"name","lfcd"},{"version","0.1.0"}};
        }else if(m=="enumerate"){
          json jtemps = json::array(); for(auto &t: temps) jtemps.push_back({{"label",t.label},{"path",t.path},{"type",t.type},{"unit",t.unit}});
          json jpwms  = json::array(); for(auto &p: pwms)  jpwms.push_back({{"label",p.label},{"pwm_path",p.pwm_path},{"enable_path",p.enable_path.value_or("")},{"tach_path",p.tach_path.value_or("")},{"writable",p.writable}});
          rsp["result"] = json{{"temps", jtemps}, {"pwms", jpwms}};
        }else if(m=="readTemp"){
          auto path = req["params"].value("path", std::string());
          rsp["result"] = read_temp_c(path);
        }else if(m=="probePwm"){
          auto label = req["params"].value("label", std::string());
          auto it = std::find_if(pwms.begin(), pwms.end(), [&](auto&x){return x.label==label;});
          if(it==pwms.end()) throw std::runtime_error("not found");
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
