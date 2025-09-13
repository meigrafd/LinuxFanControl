#include "Hwmon.hpp"
#include <filesystem>
#include <fstream>
#include <thread>
#include <map>
#include <cmath>

using namespace std;

static inline double milli_to_c(double v){ return v>200.0? v/1000.0 : v; }

string Hwmon::readText(const string& p){ try{ ifstream f(p); if(!f.good()) return {}; string s; getline(f,s,'\0'); return s; }catch(...){ return {}; } }
bool Hwmon::writeRaw(const string& p, const string& v){ try{ ofstream f(p); if(!f.good()) return false; f<<v; return true; }catch(...){ return false; } }
optional<double> Hwmon::readTempC(const string& p){ try{ ifstream f(p); if(!f.good()) return {}; string s; getline(f,s); return milli_to_c(stod(s)); }catch(...){ return {}; } }
optional<int> Hwmon::readRpm(const string& p){ try{ if(p.empty()) return {}; ifstream f(p); if(!f.good()) return {}; string s; getline(f,s); return stoi(s); }catch(...){ return {}; } }
optional<int> Hwmon::readPwmRaw(const string& p){ try{ ifstream f(p); if(!f.good()) return {}; string s; getline(f,s); return stoi(s); }catch(...){ return {}; } }
bool Hwmon::writePwmPct(const string& p, int pct, int floor_pct){ int c=max(floor_pct,min(100,pct)); int raw = (int)llround(c*255.0/100.0); return writeRaw(p,to_string(raw)); }

vector<TempSensor> Hwmon::discoverTemps(const string& base){
  vector<TempSensor> out; namespace fs = std::filesystem; if(!fs::exists(base)) return out;
  for(auto& e : fs::directory_iterator(base)){
    if(!e.is_directory()) continue; auto hw=e.path();
    string dev = readText((hw/"name").string()); if(dev.empty()) dev=hw.filename().string();
    map<string,string> label;
    for(auto& f : fs::directory_iterator(hw)){ auto fn=f.path().filename().string(); if(fn.rfind("temp",0)==0 && fn.find("_label")!=string::npos) label[fn.substr(0,fn.find('_'))]=readText(f.path().string()); }
    for(auto& f : fs::directory_iterator(hw)){
      auto fn=f.path().filename().string();
      if(fn.rfind("temp",0)==0 && fn.find("_input")!=string::npos){
        string t = fn.substr(0,fn.find('_')); string raw = label.count(t)? label[t] : t;
        out.push_back({ hw.filename().string()+":"+dev, hw.filename().string()+":"+raw, f.path().string(), "°C", "Unknown"});
      }
    }
  }
  return out;
}

vector<PwmOutput> Hwmon::discoverPwms(const string& base){
  vector<PwmOutput> out; namespace fs = std::filesystem; if(!fs::exists(base)) return out;
  for(auto& e : fs::directory_iterator(base)){
    if(!e.is_directory()) continue; auto hw=e.path();
    string dev = readText((hw/"name").string()); if(dev.empty()) dev=hw.filename().string();
    map<string,string> m; for(auto& f: fs::directory_iterator(hw)){ m[f.path().filename().string()]=f.path().string(); }
    for(const auto& [fn,full]: m){
      if(fn.rfind("pwm",0)==0 && fn.find('_')==string::npos){
        string n=fn.substr(3); string en = m.count("pwm"+n+"_enable")? m["pwm"+n+"_enable"] : ""; string tach = m.count("fan"+n+"_input")? m["fan"+n+"_input"] : "";
        out.push_back({ hw.filename().string()+":"+dev, hw.filename().string()+":"+fn, full, en, tach });
      }
    }
  }
  return out;
}

CalibResult Hwmon::calibrate(const PwmOutput& p, int start, int end, int step, double settle_s, int floor_pct, int rpm_threshold, bool (*cancelled)()){
  CalibResult r;
  if(!p.enable_path.empty()) Hwmon::writeRaw(p.enable_path,"1");
  auto read_rpm=[&](){ return Hwmon::readRpm(p.tach_path).value_or(0); };
  int spin=-1, rpm_min=0;
  for(int duty=max(start,floor_pct); duty<=end; duty+=step){
    if(cancelled && cancelled()){ r.aborted=true; return r; }
    Hwmon::writePwmPct(p.pwm_path,duty,floor_pct);
    std::this_thread::sleep_for(std::chrono::milliseconds((int)(settle_s*1000)));
    int rpm = read_rpm(); if(rpm>=rpm_threshold){ spin=duty; rpm_min=rpm; break; }
  }
  if(spin<0){ r.ok=false; r.error="no spin"; return r; }
  int min_stable=spin;
  for(int duty=spin; duty>=max(start,floor_pct); duty-=step){
    if(cancelled && cancelled()){ r.aborted=true; return r; }
    Hwmon::writePwmPct(p.pwm_path,duty,floor_pct);
    std::this_thread::sleep_for(std::chrono::milliseconds((int)(settle_s*1000)));
    int rpm = read_rpm(); if(rpm>=rpm_threshold) min_stable=duty; else break;
  }
  Hwmon::writePwmPct(p.pwm_path,min_stable,floor_pct);
  r.ok=true; r.min_pct=min_stable; r.spinup_pct=spin; r.rpm_at_min=rpm_min; return r;
}
