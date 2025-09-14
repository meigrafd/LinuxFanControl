#include "Hwmon.hpp"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <cstring>
#include <cerrno>

namespace fs = std::filesystem;

static inline std::string readFileTrim(const std::string& p) {
  std::ifstream f(p);
  if (!f) return {};
  std::ostringstream o; o << f.rdbuf();
  std::string s = o.str();
  while (!s.empty() && (s.back()=='\n' || s.back()=='\r' || s.back()==' ' || s.back()=='\t')) s.pop_back();
  return s;
}

static inline bool writeFile(const std::string& p, const std::string& v) {
  std::ofstream f(p);
  if (!f) return false;
  f << v;
  return (bool)f;
}

HwmonSnapshot hwmon::scanSysfs(const std::string& root) {
  HwmonSnapshot snap;
  for (auto& d : fs::directory_iterator(root)) {
    if (!d.is_directory()) continue;
    auto base = d.path();
    std::string chip = readFileTrim((base / "name").string());
    // sensors
    for (auto& f : fs::directory_iterator(base)) {
      auto fn = f.path().filename().string();
      if (fn.rfind("temp",0)==0 && fn.find("_input")!=std::string::npos) {
        std::string stem = fn.substr(0, fn.find("_input")); // temp1
        std::string label = readFileTrim((base / (stem + "_label")).string());
        if (label.empty()) label = chip + ":" + stem;
        HwmonSensor s;
        s.id = base.filename().string() + ":" + stem;
        s.label = label;
        s.type = "temp";
        s.path = f.path().string();
        snap.sensors.push_back(std::move(s));
      } else if (fn.rfind("fan",0)==0 && fn.find("_input")!=std::string::npos) {
        std::string stem = fn.substr(0, fn.find("_input")); // fan1
        std::string label = readFileTrim((base / (stem + "_label")).string());
        if (label.empty()) label = chip + ":" + stem;
        HwmonSensor s;
        s.id = base.filename().string() + ":" + stem;
        s.label = label;
        s.type = "fan";
        s.path = f.path().string();
        snap.sensors.push_back(std::move(s));
      } else if (fn.rfind("pwm",0)==0 && fn.find("_enable")==std::string::npos) {
        std::string stem = fn; // pwm1
        HwmonPwm p;
        p.id = base.filename().string() + ":" + stem;
        p.label = chip + ":" + stem;
        p.pathPwm = f.path().string();
        p.pathEnable = (base / (stem + "_enable")).string();
        // probe writability (do not change current value)
        p.writable = fs::exists(p.pathEnable);
        snap.pwms.push_back(std::move(p));
      }
    }
  }
  return snap;
}

std::optional<double> hwmon::readValue(const HwmonSensor& s) {
  std::ifstream f(s.path);
  if (!f) return std::nullopt;
  long long raw=0; f >> raw;
  if (!f) return std::nullopt;
  if (s.type=="temp") return (double)raw / 1000.0;
  if (s.type=="fan")  return (double)raw; // RPM
  return std::nullopt;
}

bool hwmon::writePwmRaw(const HwmonPwm& p, int value) {
  if (value < 0) value = 0;
  if (value > 255) value = 255;
  return writeFile(p.pathPwm, std::to_string(value));
}

bool hwmon::enableManual(const HwmonPwm& p) {
  if (p.pathEnable.empty()) return false;
  // "1" -> manual (common); other drivers use 2 or 3; we try 1 then 2
  if (writeFile(p.pathEnable, "1")) return true;
  (void)writeFile(p.pathEnable, "2");
  return false;
}

std::optional<int> hwmon::readPwmRaw(const HwmonPwm& p) {
  std::ifstream f(p.pathPwm);
  if (!f) return std::nullopt;
  int v=0; f >> v;
  if (!f) return std::nullopt;
  return v;
}
