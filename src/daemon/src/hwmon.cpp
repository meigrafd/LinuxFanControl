#include "hwmon.hpp"
#include <filesystem>
#include <fstream>
#include <regex>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <sensors/sensors.h>

using std::string; using std::vector;
namespace fs = std::filesystem;

static inline double clamp(double v, double lo, double hi){ return v<lo?lo:(v>hi?hi:v); }
static inline bool read_file(const string& p, string& out){
  std::ifstream f(p); if(!f) return false; std::getline(f, out, '\0'); if(!f && !f.eof()) return false; return true;
}
static inline bool write_file(const string& p, const string& s){
  std::ofstream f(p); if(!f) return false; f<<s; return (bool)f;
}
static inline double milli_to_c(double v){ return v>200.0? v/1000.0 : v; }

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

vector<TempSensor> enumerate_temps(){
  vector<TempSensor> res;
  const fs::path base("/sys/class/hwmon");
  if(!fs::exists(base)) return res;
  for(auto& dir : fs::directory_iterator(base)){
    if(!fs::is_directory(dir)) continue;
    auto name = hw_name(dir.path());
    // read labels
    std::map<string,string> labels;
    for(auto& f : fs::directory_iterator(dir)){
      auto fn = f.path().filename().string();
      if(fn.rfind("temp",0)==0 && fn.size()>6 && fn.find("_label")!=string::npos){
        string val; if(read_file(f.path().string(), val)){
          if(!val.empty() && val.back()=='\n') val.pop_back();
          labels[fn.substr(0, fn.find('_'))] = val;
        }
      }
    }
    // inputs
    for(auto& f : fs::directory_iterator(dir)){
      auto fn = f.path().filename().string();
      if(fn.rfind("temp",0)==0 && fn.size()>6 && fn.find("_input")!=string::npos){
        string tempn = fn.substr(0, fn.find('_'));
        string raw = labels.count(tempn)? labels[tempn] : tempn;
        string type = classify_type(name, raw);
        string nice = (type!="Unknown" && raw.rfind(type+":",0)!=0) ? (type + ": " + raw) : raw;
        TempSensor s;
        s.label = dir.path().filename().string() + ":" + name + ":" + nice;
        s.path  = f.path().string();
        s.type  = type;
        s.unit  = "°C";
        res.push_back(std::move(s));
      }
    }
  }
  return res;
}

vector<PwmDevice> enumerate_pwms(){
  vector<PwmDevice> res;
  const fs::path base("/sys/class/hwmon");
  if(!fs::exists(base)) return res;
  for(auto& dir : fs::directory_iterator(base)){
    if(!fs::is_directory(dir)) continue;
    auto name = hw_name(dir.path());
    for(auto& f : fs::directory_iterator(dir)){
      auto fn = f.path().filename().string();
      if(fn.rfind("pwm",0)==0 && fn.find("_enable")==string::npos){
        string num;
        for(char c: fn) if(isdigit((unsigned char)c)) num.push_back(c);
        PwmDevice d;
        d.label = dir.path().filename().string() + ":" + name + ":" + fn;
        d.pwm_path = f.path().string();
        auto en = dir.path() / (string("pwm")+num+"_enable");
        if(fs::exists(en)) d.enable_path = en.string();
        auto tach = dir.path() / (string("fan")+num+"_input");
        if(fs::exists(tach)) d.tach_path = tach.string();
        d.writable = false;
        res.push_back(std::move(d));
      }
    }
  }
  return res;
}

double read_temp_c(const string& path){
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
