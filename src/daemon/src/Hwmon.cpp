#include "Hwmon.hpp"
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cstring>
#include <fstream>
#include <sstream>

using namespace lfc;

static bool is_file(const std::string& p) {
  struct stat st{}; return ::stat(p.c_str(), &st)==0 && S_ISREG(st.st_mode);
}
static bool is_writable(const std::string& p) {
  return ::access(p.c_str(), W_OK)==0;
}

HwmonSnapshot Hwmon::scan() {
  HwmonSnapshot s;
  const char* root = "/sys/class/hwmon";
  DIR* d = ::opendir(root);
  if (!d) return s;
  while (auto* de = ::readdir(d)) {
    if (de->d_name[0]=='.') continue;
    std::string base = std::string(root) + "/" + de->d_name;
    for (int i=1;i<=8;i++) {
      std::string pwm = base + "/pwm"+std::to_string(i);
      if (is_file(pwm)) {
        HwmonPwm p;
        p.hwmon = base; p.index = i; p.path_pwm = pwm;
        p.path_en = base + "/pwm"+std::to_string(i)+"_enable";
        p.path_max= base + "/pwm"+std::to_string(i)+"_max";
        if (is_file(p.path_max)) {
          if (auto v = readInt(p.path_max)) p.max_raw = *v;
        }
        p.writable = is_writable(pwm);
        s.pwms.push_back(p);
      }
      std::string fan = base + "/fan"+std::to_string(i)+"_input";
      if (is_file(fan)) s.fans.push_back(HwmonFan{base,i,fan});
      std::string ti = base + "/temp"+std::to_string(i)+"_input";
      if (is_file(ti)) s.temps.push_back(HwmonTemp{base,i,ti});
    }
  }
  ::closedir(d);
  return s;
}

std::optional<int> Hwmon::readInt(const std::string& path) {
  std::ifstream f(path); if (!f) return std::nullopt;
  long v=0; f>>v; if (!f.good() && !f.eof()) return std::nullopt;
  return static_cast<int>(v);
}
bool Hwmon::writeInt(const std::string& path, int v) {
  std::ofstream f(path); if (!f) return false; f<<v; return f.good();
}
bool Hwmon::setManual(const HwmonPwm& p) {
  if (!is_file(p.path_en)) return true; // some drivers don't expose _enable
  return writeInt(p.path_en, 1);
}
bool Hwmon::setAuto(const HwmonPwm& p) {
  if (!is_file(p.path_en)) return true;
  return writeInt(p.path_en, 2);
}
bool Hwmon::setPercent(const HwmonPwm& p, int pct) {
  if (!p.writable) return false;
  if (pct<0) pct=0; if (pct>100) pct=100;
  int raw = (p.max_raw<=0?255:p.max_raw) * pct / 100;
  return writeInt(p.path_pwm, raw);
}
std::optional<int> Hwmon::readRpm(const HwmonFan& f) { return readInt(f.path_input); }
std::optional<int> Hwmon::readMilliC(const HwmonTemp& t) { return readInt(t.path_input); }
