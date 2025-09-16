/*
 * Linux Fan Control — HWMON abstraction (implementation)
 * - Sysfs scanning and helpers
 * - Basic PWM write and sensor read
 * (c) 2025 LinuxFanControl contributors
 */
#include "Hwmon.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <cmath>
#include <optional>
#include <string>
#include <vector>
#include <filesystem>
#include <unordered_map>
#include <fstream>
#include <unistd.h>

namespace fs = std::filesystem;
namespace lfc {

  static std::optional<std::string> readText(const std::string& path) {
    std::ifstream f(path);
    if (!f) return std::nullopt;
    std::string s; std::getline(f, s);
    return s;
  }

  std::optional<int> Hwmon::readInt(const std::string& path) {
    std::ifstream f(path);
    if (!f) return std::nullopt;
    long v = 0;
    f >> v;
    if (!f) return std::nullopt;
    return static_cast<int>(v);
  }

  bool Hwmon::writeInt(const std::string& path, int value) {
    std::ofstream f(path);
    if (!f) return false;
    f << value;
    return static_cast<bool>(f);
  }

  std::optional<int> Hwmon::readMilliC(const TempSensor& t) {
    if (auto v = readInt(t.path_input)) return *v;
    return std::nullopt;
  }

  std::optional<int> Hwmon::readTempC(const TempSensor& t) {
    int val = 0;
    if (!read_int_file(t.path_input, val)) return std::nullopt;
    return val / 1000; // assuming millidegree Celsius
  }

  std::optional<int> Hwmon::readRpm(const HwmonFan& f) {
    if (auto v = readInt(f.path_input)) return *v;
    return std::nullopt;
  }

  void Hwmon::setManual(const HwmonPwm& p) {
    // 1 = manual on most kernels; if different, your engine can adapt
    (void)writeInt(p.path_enable, 1);
  }

  void Hwmon::setPercent(const HwmonPwm& p, int percent) {
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    // map 0..100 -> 0..255 (common pwm range)
    int raw = static_cast<int>(std::lround(percent * 255.0 / 100.0));
    (void)writeInt(p.path_pwm, raw);
  }

  static std::vector<TempSensor> discoverTemps(const std::string& hwmonPath) {
    std::vector<TempSensor> out;
    std::unordered_map<std::string, std::string> labels;
    for (int i = 1; i <= 16; ++i) {
      std::string base = hwmonPath + "/temp" + std::to_string(i);
      std::string in   = base + "_input";
      std::string lbl  = base + "_label";
      if (!fs::exists(in)) continue;
      TempSensor t;
      t.hwmon = hwmonPath;
      t.index = i;
      t.path_input = in;
      if (fs::exists(lbl)) {
        if (auto s = readText(lbl)) t.label = *s;
      }
      out.push_back(std::move(t));
    }
    (void)labels;
    return out;
  }

  static std::vector<HwmonFan> discoverFans(const std::string& hwmonPath) {
    std::vector<HwmonFan> out;
    for (int i = 1; i <= 16; ++i) {
      std::string in = hwmonPath + "/fan" + std::to_string(i) + "_input";
      if (!fs::exists(in)) continue;
      HwmonFan f;
      f.hwmon = hwmonPath;
      f.index = i;
      f.path_input = in;
      out.push_back(std::move(f));
    }
    return out;
  }

  static std::vector<HwmonPwm> discoverPwms(const std::string& hwmonPath) {
    std::vector<HwmonPwm> out;
    for (int i = 1; i <= 16; ++i) {
      std::string pwm    = hwmonPath + "/pwm" + std::to_string(i);
      std::string enable = hwmonPath + "/pwm" + std::to_string(i) + "_enable";
      if (!fs::exists(pwm) || !fs::exists(enable)) continue;
      HwmonPwm p;
      p.hwmon = hwmonPath;
      p.index = i;
      p.path_pwm = pwm;
      p.path_enable = enable;
      out.push_back(std::move(p));
    }
    return out;
  }

  HwmonSnapshot Hwmon::scan() {
    HwmonSnapshot s;
    const std::string root = "/sys/class/hwmon";
    if (!fs::exists(root)) return s;
    for (auto& e : fs::directory_iterator(root)) {
      if (!e.is_directory()) continue;
      std::string path = e.path().string();
      // follow device symlink if needed
      if (fs::is_symlink(e.path())) {
        std::error_code ec;
        auto r = fs::read_symlink(e.path(), ec);
        if (!ec) path = (e.path().parent_path() / r).string();
      }
      auto temps = discoverTemps(path);
      auto fans  = discoverFans(path);
      auto pwms  = discoverPwms(path);
      s.temps.insert(s.temps.end(), temps.begin(), temps.end());
      s.fans.insert(s.fans.end(), fans.begin(), fans.end());
      s.pwms.insert(s.pwms.end(), pwms.begin(), pwms.end());
    }
    return s;
  }

} // namespace lfc
