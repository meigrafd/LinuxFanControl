// LinuxFanControl - Daemon hwmon backend (implementation)
// Comments in English. This file intentionally includes <unistd.h> to fix ::access/W_OK build error.

#include "Hwmon.h"

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <optional>
#include <regex>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include <unistd.h> // <- required for ::access(), W_OK, R_OK, F_OK

using std::optional;
using std::string;
using std::string_view;
namespace fs = std::filesystem;

namespace lfc {

  static inline string read_whole(const string& path) {
    std::ifstream f(path);
    if (!f.good()) return {};
    std::string s; std::getline(f, s, '\0');
    // trim trailing newlines
    while (!s.empty() && (s.back() == '\n' || s.back() == '\r')) s.pop_back();
    return s;
  }

  static inline bool write_whole(const string& path, string_view text) {
    std::ofstream f(path);
    if (!f.good()) return false;
    f << text;
    return bool(f);
  }

  bool Hwmon::Exists(const string& path) {
    std::error_code ec;
    return fs::exists(path, ec);
  }

  bool Hwmon::IsWritable(const string& path) {
    // Fixes build error by using <unistd.h> for ::access and W_OK
    return ::access(path.c_str(), W_OK) == 0;
  }

  optional<string> Hwmon::ReadText(const string& path) {
    if (!Exists(path)) return std::nullopt;
    auto s = read_whole(path);
    if (s.empty()) return std::nullopt;
    return s;
  }

  bool Hwmon::WriteText(const string& path, string_view text) {
    return write_whole(path, text);
  }

  double Hwmon::MilliToC(double v) {
    return (v > 200.0) ? (v / 1000.0) : v;
  }

  optional<double> Hwmon::ReadTempC(const string& temp_input_path) {
    try {
      auto s = read_whole(temp_input_path);
      if (s.empty()) return std::nullopt;
      double raw = std::stod(s);
      return MilliToC(raw);
    } catch (...) {
      return std::nullopt;
    }
  }

  optional<int> Hwmon::ReadRpm(const string& tach_path) {
    if (tach_path.empty()) return std::nullopt;
    try {
      auto s = read_whole(tach_path);
      if (s.empty()) return std::nullopt;
      return std::stoi(s);
    } catch (...) {
      return std::nullopt;
    }
  }

  optional<int> Hwmon::ReadPwmRaw(const string& pwm_path) {
    try {
      auto s = read_whole(pwm_path);
      if (s.empty()) return std::nullopt;
      int v = std::stoi(s);
      if (v < 0) return std::nullopt;
      return v;
    } catch (...) {
      return std::nullopt;
    }
  }

  bool Hwmon::WritePwmRaw(const string& pwm_path, int raw) {
    if (raw < 0) raw = 0;
    if (raw > 255) raw = 255;
    return write_whole(pwm_path, std::to_string(raw));
  }

  bool Hwmon::WritePwmPercent(const string& pwm_path, double percent) {
    if (percent < 0.0) percent = 0.0;
    if (percent > 100.0) percent = 100.0;
    int raw = int(std::lround(percent * 255.0 / 100.0));
    return WritePwmRaw(pwm_path, raw);
  }

  optional<std::string> Hwmon::ReadEnable(const std::string& enable_path) {
    if (enable_path.empty()) return std::nullopt;
    auto s = ReadText(enable_path);
    if (!s || s->empty()) return std::nullopt;
    return s;
  }

  bool Hwmon::WriteEnable(const std::string& enable_path, std::string_view v) {
    if (enable_path.empty()) return false;
    return WriteText(enable_path, v);
  }

  // --- Classification helpers (heuristics similar to the python prototype) ---
  static bool re_any(std::string_view text, const std::vector<std::string>& pats) {
    std::string t{text};
    for (auto& p : pats) {
      try {
        if (std::regex_search(t, std::regex(p, std::regex::icase))) return true;
      } catch (...) {}
    }
    return false;
  }

  static std::string classify_sensor(std::string_view hwmon_name,
                                     std::string_view temp_label,
                                     const fs::path& device_symlink) {
    const std::vector<std::string> name_cpu = {R"(k10temp)", R"(coretemp)", R"(zenpower)", R"(pkgtemp)"};
    const std::vector<std::string> name_gpu = {R"(amdgpu)", R"(nvidia)"};
    const std::vector<std::string> name_nvme= {R"(nvme)"};
    const std::vector<std::string> name_ec  = {R"(asus[-_]?ec)", R"(ibm-ec)", R"(\bec\b)"};
    const std::vector<std::string> label_cpu= {R"(\bcpu\b)", R"(tctl)", R"(package)", R"(tdie)", R"(core)"};
    const std::vector<std::string> label_gpu= {R"(\bgpu\b)", R"(junction)", R"(hotspot)", R"(edge)", R"(vram)", R"(hbm)"};
    const std::vector<std::string> label_chip= {R"(chip|pch|smu|south|north)"};
    const std::vector<std::string> label_mobo= {R"(mobo|mb|board|system|systin|case)"};
    const std::vector<std::string> label_vrm = {R"(vrm|mos|vcore)"};
    const std::vector<std::string> label_nvme= {R"(nvme|ssd|composite)"};
    const std::vector<std::string> label_water={R"(water|coolant|liquid)"};
    const std::vector<std::string> label_amb= {R"(ambient|room)"};

    const std::string name = std::string(hwmon_name);
    const std::string lab  = std::string(temp_label);
    const std::string devp = device_symlink.string();

    // Device path hints first
    if (!devp.empty()) {
      if (devp.find("/drm/") != std::string::npos || devp.find("amdgpu") != std::string::npos || devp.find("nvidia") != std::string::npos)
        return "GPU";
      if (devp.find("/nvme") != std::string::npos || devp.find("/block/nvme") != std::string::npos)
        return "NVMe";
    }

    if (re_any(name, name_cpu)) return "CPU";
    if (re_any(name, name_gpu)) return "GPU";
    if (re_any(name, name_nvme)) return "NVMe";
    if (re_any(lab,  label_cpu)) return "CPU";
    if (re_any(lab,  label_gpu)) return "GPU";
    if (re_any(lab,  label_chip)) return "Chipset";
    if (re_any(lab,  label_mobo)) return "Motherboard";
    if (re_any(lab,  label_vrm))  return "VRM";
    if (re_any(lab,  label_nvme)) return "NVMe";
    if (re_any(lab,  label_water))return "Water";
    if (re_any(lab,  label_amb))  return "Ambient";

    if (re_any(name, name_ec)) return "Motherboard";
    return "Unknown";
}

static std::string suggest_label(const std::string& type, const std::string& base_label) {
  if (base_label.empty()) return type.empty() ? "Unknown" : type;
  std::string bl = base_label;
  std::string lower = bl; std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
  std::string prefix = type; std::transform(prefix.begin(), prefix.end(), prefix.begin(), ::tolower);
  if (!type.empty() && lower.rfind(prefix + ":", 0) != 0) {
    return type + ": " + bl;
  }
  return bl;
}

std::vector<TempSensor> Hwmon::DiscoverTemps(const std::string& base) {
  std::vector<TempSensor> out;
  std::error_code ec;
  if (!fs::exists(base, ec) || !fs::is_directory(base, ec)) return out;

  for (auto& dir : fs::directory_iterator(base, ec)) {
    if (ec) break;
    if (!dir.is_directory()) continue;
    const auto hwdir = dir.path();
    std::string hwid = hwdir.filename().string();
    std::string hwname = read_whole((hwdir / "name").string());
    if (hwname.empty()) hwname = hwid;

    // resolve device symlink for classification
    fs::path devlink = fs::weakly_canonical(hwdir / "device", ec);

    // collect labels
    std::unordered_map<std::string, std::string> label_map;
    for (auto& f : fs::directory_iterator(hwdir, ec)) {
      if (ec) break;
      auto fn = f.path().filename().string();
      if (fn.rfind("temp", 0) == 0 && fn.size() > 5 && fn.find("_label") != std::string::npos) {
        auto key = fn.substr(0, fn.find('_')); // tempN
        label_map[key] = read_whole(f.path().string());
      }
    }
    for (auto& f : fs::directory_iterator(hwdir, ec)) {
      if (ec) break;
      auto fn = f.path().filename().string();
      if (fn.rfind("temp", 0) == 0 && fn.size() > 6 && fn.find("_input") != std::string::npos) {
        auto key = fn.substr(0, fn.find('_')); // tempN
        std::string raw = label_map.count(key) ? label_map[key] : key;
        std::string typ = classify_sensor(hwname, raw, devlink);
        std::string nice = suggest_label(typ, raw);
        TempSensor s;
        s.device    = hwid + ":" + hwname;
        s.label     = s.device + ":" + nice;
        s.raw_label = raw;
        s.type      = typ;
        s.path      = f.path().string();
        s.unit      = "°C";
        out.push_back(std::move(s));
      }
    }
  }
  return out;
}

std::vector<PwmDevice> Hwmon::DiscoverPwms(const std::string& base) {
  std::vector<PwmDevice> out;
  std::error_code ec;
  if (!fs::exists(base, ec) || !fs::is_directory(base, ec)) return out;

  for (auto& dir : fs::directory_iterator(base, ec)) {
    if (ec) break;
    if (!dir.is_directory()) continue;
    const auto hwdir = dir.path();
    std::string hwid = hwdir.filename().string();
    std::string hwname = read_whole((hwdir / "name").string());
    if (hwname.empty()) hwname = hwid;

    // gather files
    std::unordered_map<std::string, std::string> files;
    for (auto& f : fs::directory_iterator(hwdir, ec)) {
      if (ec) break;
      auto fn = f.path().filename().string();
      files[fn] = f.path().string();
    }
    // collect pwmN, pwmN_enable, fanN_input
    for (auto& [fn, full] : files) {
      if (fn.rfind("pwm", 0) == 0 && fn.size() >= 4 && std::all_of(fn.begin() + 3, fn.end(), ::isdigit)) {
        std::string n = fn.substr(3);
        PwmDevice d;
        d.device = hwid + ":" + hwname;
        d.label  = d.device + ":" + fn;
        d.pwm_path = full;
        auto en_it = files.find("pwm" + n + "_enable");
        if (en_it != files.end()) d.enable_path = en_it->second;
        auto tach_it = files.find("fan" + n + "_input");
        if (tach_it != files.end()) d.tach_path = tach_it->second;
        out.push_back(std::move(d));
      }
    }
  }
  return out;
}

} // namespace lfc
