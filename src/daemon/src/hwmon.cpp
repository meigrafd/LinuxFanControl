// All comments in English by request.

#include "hwmon.hpp"
#include <filesystem>
#include <fstream>
#include <regex>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <sensors/sensors.h>
#include <chrono>
#include <cmath>
#include <map>
#include <atomic>
#include <cstdio>
#include <set>
#include <algorithm>

using std::string; using std::vector;
namespace fs = std::filesystem;

static inline double clamp(double v, double lo, double hi){ return v<lo?lo:(v>hi?hi:v); }
static inline double milli_to_c(double v){ return v>200.0? v/1000.0 : v; }

bool read_file_str(const string& p, string& out){
    std::ifstream f(p);
    if(!f) return false;
    std::getline(f, out, '\0');
    if(!f && !f.eof()) return false;
    return true;
}
bool write_file_str(const string& p, const string& s){
    std::ofstream f(p);
    if(!f) return false;
    f<<s;
    return (bool)f;
}
static inline bool read_file(const string& p, string& out){ return read_file_str(p,out); }
static inline bool write_file(const string& p, const string& s){ return write_file_str(p,s); }

static string hw_name(const fs::path& hw){
    string n; if(read_file(hw/"name", n)){ if(!n.empty() && n.back()=='\n') n.pop_back(); return n; }
    return hw.filename().string();
}

static string classify_type(const string& devname, const string& label){
    auto low=[&](string s){ for(char&c: s) c=tolower(c); return s; };
    string dn=low(devname), lb=low(label);
    auto m=[&](const string& pat, const string& s){ return std::regex_search(s, std::regex(pat)); };
    if(m("amdgpu|nvidia", dn)) return "GPU";
    if(m("k10temp|coretemp|zen|pkg", dn) || m("\\bcpu\\b|tctl|tdie|package", lb)) return "CPU";
    if(m("nvme", dn) || m("\\bnvme\\b", lb)) return "NVMe";
    if(m("nct|it8|w83", dn)) return "Motherboard";
    if(m("vrm|mos", lb)) return "VRM";
    if(m("water|coolant|liquid", lb)) return "Water";
    if(m("ambient|room", lb)) return "Ambient";
    return "Unknown";
}

// ----- libsensors helpers -----
static std::atomic<bool> s_ls_inited{false};
static void ensure_libsensors(){
    if(!s_ls_inited.load()){
        if(sensors_init(NULL)==0) s_ls_inited.store(true);
    }
}
static std::string chip_to_string(const sensors_chip_name* chip){
    char buf[512]; buf[0]=0;
    if(sensors_snprintf_chip_name(buf, sizeof(buf), chip)>=0) return std::string(buf);
    return "unknown";
}

// ---------- Enumeration (temps) ----------
std::vector<TempSensor> enumerate_temps(){
    vector<TempSensor> res;
    const fs::path base("/sys/class/hwmon");
    if(fs::exists(base)){
        for(auto &entry : fs::directory_iterator(base)){
            if(!entry.is_directory()) continue;
            auto hw = entry.path();
            string devname = hw_name(hw);
            // collect labels
            std::map<string,string> labels;
            for(auto &fe : fs::directory_iterator(hw)){
                auto fn = fe.path().filename().string();
                if(std::regex_match(fn, std::regex(R"(temp(\d+)_label)"))){
          string lab; if(read_file(fe.path().string(), lab)){
            if(!lab.empty() && lab.back()=='\n') lab.pop_back();
            auto key = fn.substr(0, fn.find('_'));
            labels[key] = lab;
          }
                }
            }
      for(auto &fe : fs::directory_iterator(hw)){
        auto fn = fe.path().filename().string();
        if(std::regex_match(fn, std::regex(R"(temp(\d+)_input)"))){
          auto key = fn.substr(0, fn.find('_'));
          string lab = labels.count(key)? labels[key] : key;
          string type = classify_type(devname, lab);
          TempSensor t;
          t.label = hw.filename().string() + ":" + devname + ":" + lab;
          t.path  = fe.path().string();
          t.type  = type;
          t.unit  = "°C";
          res.push_back(std::move(t));
        }
      }
        }
    }

  // Add libsensors entries (ls://chip#feature_number). De-duplicate by label.
  ensure_libsensors();
  int c=0; const sensors_chip_name* chip;
  std::set<std::string> have; for(auto &t: res) have.insert(t.label);
  while((chip = sensors_get_detected_chips(NULL, &c)) != NULL){
    std::string chipname = chip_to_string(chip);
    int f=0; const sensors_feature* feat;
    while((feat = sensors_get_features(chip, &f)) != NULL){
      if(feat->type != SENSORS_FEATURE_TEMP) continue;
      const char* lbl = sensors_get_label(chip, feat);
      std::string label = std::string("libsensors:")+chipname+":"+ (lbl?lbl:feat->name);
      if(have.count(label)) continue;
      TempSensor t;
        t.label = label;
        t.path  = std::string("ls://")+chipname+"#"+std::to_string(feat->number);
        t.type  = classify_type(chipname, lbl?lbl:feat->name);
        t.unit  = "°C";
        res.push_back(std::move(t));
    }
  }
  return res;
}

// ---------- Enumeration (pwms) ----------
std::vector<PwmDevice> enumerate_pwms(){
    vector<PwmDevice> res;
    const fs::path base("/sys/class/hwmon");
    if(!fs::exists(base)) return res;
    for(auto &entry : fs::directory_iterator(base)){
        if(!entry.is_directory()) continue;
        auto hw = entry.path();
        string devname = hw_name(hw);
        // gather pwm, enable, tach by index
        std::map<int,string> pwm, en, tach;
        for(auto &fe : fs::directory_iterator(hw)){
            auto fn = fe.path().filename().string();
            std::smatch m;
            if(std::regex_match(fn, m, std::regex(R"(pwm(\d+))"))){
        int n = std::stoi(m[1].str()); pwm[n] = fe.path().string();
            }else if(std::regex_match(fn, m, std::regex(R"(pwm(\d+)_enable)"))){
        int n = std::stoi(m[1].str()); en[n] = fe.path().string();
            }else if(std::regex_match(fn, m, std::regex(R"(fan(\d+)_input)"))){
        int n = std::stoi(m[1].str()); tach[n] = fe.path().string();
            }
        }
    for(auto &[n, p] : pwm){
      PwmDevice d;
      d.label = hw.filename().string() + ":" + devname + ":pwm" + std::to_string(n);
      d.pwm_path = p;
      if(en.count(n))   d.enable_path = en[n];
      if(tach.count(n)) d.tach_path   = tach[n];
      d.writable = false;
      res.push_back(std::move(d));
    }
    }
  return res;
}

// ---------- IO ----------
double read_temp_c(const string& path){
  // support libsensors scheme: ls://<chip>#<feature_number>
  if(path.rfind("ls://", 0)==0){
    ensure_libsensors();
    auto rest = path.substr(5);
    auto hash = rest.rfind('#');
    if(hash==std::string::npos) throw std::runtime_error("invalid ls path: "+path);
    std::string chipname = rest.substr(0, hash);
    int featnum = std::stoi(rest.substr(hash+1));
    int c=0; const sensors_chip_name* chip;
    while((chip=sensors_get_detected_chips(NULL,&c))!=NULL){
        if(chip_to_string(chip)==chipname){
            int f=0; const sensors_feature* feat;
            while((feat=sensors_get_features(chip,&f))!=NULL){
                if(feat->number!=featnum) continue;
                auto sub = sensors_get_subfeature(chip, feat, SENSORS_SUBFEATURE_TEMP_INPUT);
                if(!sub) throw std::runtime_error("no input subfeature");
                double val=0.0;
                if(sensors_get_value(chip, sub->number, &val)!=0) throw std::runtime_error("sensors_get_value failed");
                return milli_to_c(val);
            }
        }
    }
    throw std::runtime_error("chip/feature not found for "+path);
  }
  string s; if(!read_file(path, s)) throw std::runtime_error("read failed: "+path);
  double v = std::stod(s); return milli_to_c(v);
}

int read_rpm(const string& path){
    string s; if(!read_file(path, s)) throw std::runtime_error("read failed: "+path);
    return std::stoi(s);
}

bool set_pwm_enable(const string& enable_path, int mode){
    return write_file(enable_path, std::to_string(mode));
}

bool write_pwm_pct(const string& pwm_path, double pct){
    int raw = (int)std::lround(clamp(pct,0,100)*255.0/100.0);
    return write_file(pwm_path, std::to_string(raw));
}

bool probe_pwm_writable(PwmDevice &dev, string &reason){
    reason.clear();
    string prev_raw, prev_en;
    if(dev.enable_path){
        read_file(*dev.enable_path, prev_en);
        // try manual
        set_pwm_enable(*dev.enable_path, 1);
    }
    read_file(dev.pwm_path, prev_raw);
    if(prev_raw.empty()) prev_raw = "128";
    // try a no-op write
    if(!write_file(dev.pwm_path, prev_raw)){
        if(dev.enable_path && !prev_en.empty())
            set_pwm_enable(*dev.enable_path, std::stoi(prev_en));
        reason = "write failed (operation not supported or permission denied)";
        dev.writable = false;
        return false;
    }
    // restore
    write_file(dev.pwm_path, prev_raw);
    if(dev.enable_path && !prev_en.empty())
        set_pwm_enable(*dev.enable_path, std::stoi(prev_en));
    dev.writable = true;
    return true;
}

// ---------- Curves ----------
double Curve::eval(double x) const {
    if(pts.empty()) return 0.0;
    auto P = pts; std::sort(P.begin(), P.end(), [](auto&a,auto&b){return a.first<b.first;});
    if(x<=P.front().first) return clamp(P.front().second,0,100);
    if(x>=P.back().first)  return clamp(P.back().second,0,100);
    for(size_t i=1;i<P.size();++i){
        auto [x0,y0]=P[i-1]; auto [x1,y1]=P[i];
        if(x0<=x && x<=x1){
            double t=(x-x0)/std::max(1e-9, (x1-x0));
            return clamp(y0 + t*(y1-y0), 0, 100);
        }
    }
    return clamp(P.back().second,0,100);
}

double SchmittSlew::step(const Curve& c, double x, double now_s) const {
    if(last_x.has_value()) dir_up = (x > *last_x);
    double x_eff = x + (dir_up ? +hyst_c/2.0 : -hyst_c/2.0);
    double target = c.eval(x_eff);
    last_x = x;
    if(tau_s<=0.0){
        last_y = target; t_last = now_s; return last_y;
    }
    if(t_last<=0.0){ t_last = now_s; last_y = target; return last_y; }
    double dt = std::max(0.0, now_s - t_last); t_last = now_s;
    if(dt<=0.0) return last_y;
    double alpha = 1.0 - std::exp(-dt / tau_s);
    last_y = last_y + alpha*(target - last_y);
    return clamp(last_y, 0.0, 100.0);
}

// ---------- Calibration ----------
CalibResult calibrate_pwm(const PwmDevice& dev, int rpm_threshold, int floor_pct, int step, double settle_s, double boost_hold_s){
    CalibResult out;
    // snapshot
    string prev_en, prev_pwm;
    if(dev.enable_path) read_file(*dev.enable_path, prev_en);
    read_file(dev.pwm_path, prev_pwm);
    auto restore=[&](){
        if(dev.enable_path && !prev_en.empty()) write_file(*dev.enable_path, prev_en);
        if(!prev_pwm.empty()) write_file(dev.pwm_path, prev_pwm);
    };
        // set manual
        if(dev.enable_path) set_pwm_enable(*dev.enable_path, 1);
        // 1) Boost 100% hold
        write_pwm_pct(dev.pwm_path, 100.0);
    usleep((useconds_t)(boost_hold_s*1e6));
    // 2) Step-down to min stable
    int min_stable = 100;
    for(int duty=100; duty>=std::max(floor_pct,0); duty-=std::max(1,step)){
        write_pwm_pct(dev.pwm_path, duty);
        usleep((useconds_t)(settle_s*1e6));
        try{
            if(dev.tach_path){
                int rpm = read_rpm(*dev.tach_path);
                if(rpm >= rpm_threshold) min_stable = duty;
                else break;
            }else{
                min_stable = std::max(floor_pct, duty);
            }
        }catch(...){
            min_stable = std::max(floor_pct, duty);
            break;
        }
    }
    int spinup = std::min(100, min_stable + std::max(5,step));
    write_pwm_pct(dev.pwm_path, min_stable);
    restore();
    out.ok=true; out.min_pct=min_stable; out.spinup_pct=spinup; return out;
}

// ---------- Active Coupling Detection ----------
std::vector<std::pair<std::string, CoupleRec>>
detect_coupling(const std::vector<PwmDevice>& pwms,
                const std::vector<TempSensor>& temps,
                double hold_s, double min_delta_c, int rpm_delta_threshold){
    std::vector<std::pair<std::string, CoupleRec>> result;
    auto read_all = [&](std::map<string,double>& M){
        M.clear();
        for(auto &t: temps){
            try{ M[t.path] = read_temp_c(t.path); }catch(...){}
        }
    };
    for(auto &p: pwms){
        string prev_en, prev_pwm; read_file(p.pwm_path, prev_pwm);
        if(p.enable_path) read_file(*p.enable_path, prev_en);
        if(p.enable_path) set_pwm_enable(*p.enable_path, 1);
        if(!write_pwm_pct(p.pwm_path, 100.0)){
            if(p.enable_path && !prev_en.empty()) set_pwm_enable(*p.enable_path, std::stoi(prev_en));
            if(!prev_pwm.empty()) write_file(p.pwm_path, prev_pwm);
            continue;
        }
        int rpm0=-1, rpm1=-1;
        if(p.tach_path){
            try{ rpm0=read_rpm(*p.tach_path); }catch(...){}
            usleep(700000);
            try{ rpm1=read_rpm(*p.tach_path); }catch(...){}
            if(rpm0>=0 && rpm1>=0 && std::abs(rpm1-rpm0)<rpm_delta_threshold){
                hold_s = std::min(hold_s, 2.0); // if fan doesn't react on tach, shorten hold
            }
        }
        std::map<string,double> T0, T1; read_all(T0);
        usleep((useconds_t)(hold_s*1e6));
        read_all(T1);
        string best_path; double best_score=0.0; string best_label;
        for(auto &t: temps){
            double a = T0.count(t.path)? T0[t.path]:NAN;
            double b = T1.count(t.path)? T1[t.path]:NAN;
            if(std::isnan(a) || std::isnan(b)) continue;
            double d = std::fabs(b-a);
            if(d >= min_delta_c && d > best_score){ best_score=d; best_path=t.path; best_label=t.label; }
        }
        write_file(p.pwm_path, prev_pwm);
        if(p.enable_path && !prev_en.empty()) set_pwm_enable(*p.enable_path, std::stoi(prev_en));
        if(!best_path.empty()){
            result.push_back({p.label, CoupleRec{best_path, best_label, best_score}});
        }
    }
    return result;
                }
