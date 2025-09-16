/*
 * Linux Fan Control — HWMON abstraction (implementation)
 * - Sysfs discovery (temps, fans, pwms)
 * - Optional libsensors discovery (if built)
 * (c) 2025 LinuxFanControl contributors
 */
#include "Hwmon.hpp"

#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <vector>
#include <unordered_map>
#include <cmath>
#include <cstring>

#ifdef HAVE_SENSORS
  #include <sensors/sensors.h>
#endif

namespace fs = std::filesystem;

namespace lfc {

// ---------- generic helpers ----------

static inline double round2(double v) {
    return std::round(v * 100.0) / 100.0;
}

static std::optional<std::string> readText(const std::string& path) {
    std::ifstream f(path);
    if (!f) return std::nullopt;
    std::string s;
    std::getline(f, s);
    if (!f && !f.eof()) return std::nullopt;
    // trim trailing CR/LF
    while (!s.empty() && (s.back() == '\n' || s.back() == '\r')) s.pop_back();
    return s;
}

// ---------- public file I/O ----------

std::optional<int> Hwmon::readInt(const std::string& path) {
  std::ifstream f(path);
  if (!f) return std::nullopt;
  int v{};
  if (!(f >> v)) return std::nullopt;
  return v;
}

bool Hwmon::writeInt(const std::string& path, int value) {
  std::ofstream f(path);
  if (!f) return false;
  f << value << "\n";
  return true;
}

// ---------- temperature reading ----------

static std::optional<int> readMilliC_sysfs_path(const std::string& path) {
  std::ifstream f(path);
  if (!f) return std::nullopt;
  long long mC{};
  if (!(f >> mC)) return std::nullopt;
  return static_cast<int>(mC);
}

#ifdef HAVE_SENSORS
static bool ensure_libsensors_init() {
    static bool tried = false;
    static bool ok = false;
    if (!tried) {
        tried = true;
        ok = (sensors_init(nullptr) == 0);
    }
    return ok;
}
#endif

std::optional<int> Hwmon::readMilliC(const TempSensor& t) {
  // libsensors token?
  if (t.path_input.rfind("sensors:", 0) == 0) {
#ifdef HAVE_SENSORS
    if (!ensure_libsensors_init()) return std::nullopt;
    const char* want = t.path_input.c_str() + 9; // skip "sensors:"
    const sensors_chip_name* chip;
    int c = 0;
    while ((chip = sensors_get_detected_chips(nullptr, &c)) != nullptr) {
      const sensors_feature* feature;
      int f = 0;
      while ((feature = sensors_get_features(chip, &f)) != nullptr) {
        if (feature->type != SENSORS_FEATURE_TEMP) continue;
        const sensors_subfeature* sf_input =
            sensors_get_subfeature(chip, feature, SENSORS_SUBFEATURE_TEMP_INPUT);
        const char* label = sensors_get_label(chip, feature);
        if (!sf_input || !label) continue;
        if (std::strcmp(label, want) == 0) {
          double valC = 0.0;
          if (sensors_get_value(chip, sf_input->number, &valC) == 0) {
            long long mC = static_cast<long long>(valC * 1000.0);
            return static_cast<int>(mC);
          }
        }
      }
    }
#else
    (void)t;
#endif
    return std::nullopt;
  }

  // sysfs token (path to temp*_input)
  return readMilliC_sysfs_path(t.path_input);
}

std::optional<int> Hwmon::readTempC(const TempSensor& t) {
  auto mC = readMilliC(t);
  if (!mC) return std::nullopt;
  return *mC / 1000; // integer °C (legacy helper)
}

std::optional<double> Hwmon::readTempC2dp(const TempSensor& t) {
  auto mC = readMilliC(t);
  if (!mC) return std::nullopt;
  double c = static_cast<double>(*mC) / 1000.0;
  return round2(c); // two decimals
}

// ---------- fans / pwm ----------

std::optional<int> Hwmon::readRpm(const HwmonFan& f) {
  if (auto v = readInt(f.path_input)) return *v;
  return std::nullopt;
}

std::optional<int> Hwmon::readPercent(const HwmonPwm& p) {
  // Many drivers expose duty as 0..255 with optional *_max; normalize to 0..100
  std::ifstream f(p.path_pwm);
  if (!f) return std::nullopt;
  int raw = 0;
  f >> raw;
  if (!f) return std::nullopt;

  int maxv = 255;
  std::string maxPath = p.path_pwm + "_max";
  std::ifstream fmax(maxPath);
  if (fmax) {
      int tmp = 0;
      fmax >> tmp;
      if (tmp > 0) maxv = tmp;
  }

  int pct = static_cast<int>(std::lround((100.0 * raw) / maxv));
  return std::clamp(pct, 0, 100);
}

void Hwmon::setManual(const HwmonPwm& p) {
  // 1 = manual on most kernels; if different, the engine may adapt per driver
  (void)writeInt(p.path_enable, 1);
}

void Hwmon::setPercent(const HwmonPwm& p, int percent) {
  if (percent < 0) percent = 0;
  if (percent > 100) percent = 100;

  // Map 0..100 -> raw range (default 0..255, with optional *_max)
  int maxv = 255;
  std::string maxPath = p.path_pwm + "_max";
  if (auto mv = readInt(maxPath)) maxv = std::max(1, *mv);

  int raw = static_cast<int>(std::lround(percent * (double)maxv / 100.0));
  (void)writeInt(p.path_pwm, raw);
}

// ---------- discovery ----------

static std::vector<TempSensor> discoverTemps_sysfs(const std::string& hwmonPath) {
  std::vector<TempSensor> out;
  for (int i = 1; i <= 32; ++i) {
    std::string base = hwmonPath + "/temp" + std::to_string(i);
    std::string in   = base + "_input";
    if (!fs::exists(in)) continue;

    TempSensor t;
    t.hwmon = hwmonPath;
    t.index = i;
    t.path_input = in;
    if (auto s = readText(base + "_label")) t.label = *s;
    out.push_back(std::move(t));
  }
  return out;
}

static std::vector<HwmonFan> discoverFans_sysfs(const std::string& hwmonPath) {
  std::vector<HwmonFan> out;
  for (int i = 1; i <= 32; ++i) {
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

static std::vector<HwmonPwm> discoverPwms_sysfs(const std::string& hwmonPath) {
  std::vector<HwmonPwm> out;
  for (int i = 1; i <= 32; ++i) {
    std::string pwm = hwmonPath + "/pwm" + std::to_string(i);
    std::string en  = pwm + "_enable";
    if (!fs::exists(pwm) || !fs::exists(en)) continue;
    HwmonPwm p;
    p.hwmon = hwmonPath;
    p.index = i;
    p.path_pwm = pwm;
    p.path_enable = en;
    out.push_back(std::move(p));
  }
  return out;
}

#ifdef HAVE_SENSORS
static std::vector<TempSensor> discoverTemps_libsensors() {
  std::vector<TempSensor> out;
  if (!ensure_libsensors_init()) return out;

  const sensors_chip_name* chip;
  int c = 0;
  int idx = 0;
  while ((chip = sensors_get_detected_chips(nullptr, &c)) != nullptr) {
    const sensors_feature* feature;
    int f = 0;
    while ((feature = sensors_get_features(chip, &f)) != nullptr) {
      if (feature->type != SENSORS_FEATURE_TEMP) continue;
      const sensors_subfeature* sf_input =
          sensors_get_subfeature(chip, feature, SENSORS_SUBFEATURE_TEMP_INPUT);
      const char* label = sensors_get_label(chip, feature);
      if (!sf_input || !label) continue;

      TempSensor t;
      t.hwmon = "libsensors";
      t.index = ++idx;
      t.path_input = std::string("sensors:") + label; // token for readMilliC
      t.label = label;
      out.push_back(std::move(t));
    }
  }
  return out;
}
#endif

HwmonSnapshot Hwmon::scan() {
  HwmonSnapshot s;

  const std::string root = "/sys/class/hwmon";
  if (fs::exists(root)) {
    for (auto& e : fs::directory_iterator(root)) {
      if (!e.is_directory()) continue;

      // follow device symlink if present
      std::string base = e.path().string();
      if (fs::is_symlink(e.path())) {
        std::error_code ec;
        auto r = fs::read_symlink(e.path(), ec);
        if (!ec) base = (e.path().parent_path() / r).string();
      }

      auto temps = discoverTemps_sysfs(base);
      auto fans  = discoverFans_sysfs(base);
      auto pwms  = discoverPwms_sysfs(base);
      s.temps.insert(s.temps.end(), temps.begin(), temps.end());
      s.fans.insert(s.fans.end(), fans.begin(), fans.end());
      s.pwms.insert(s.pwms.end(), pwms.begin(), pwms.end());
    }
  }

#ifdef HAVE_SENSORS
  // Add libsensors temps as additional sources
  {
    auto lt = discoverTemps_libsensors();
    s.temps.insert(s.temps.end(), lt.begin(), lt.end());
  }
#endif

  return s;
}

} // namespace lfc
