#pragma once
// Minimal /sys/class/hwmon access layer (no libsensors dependency)

#include <string>
#include <vector>
#include <optional>
#include <cstdint>

namespace lfc {

  struct HwmonPwm {
    std::string hwmon;      // e.g. /sys/class/hwmon/hwmon4
    int index;              // pwmN
    std::string path_pwm;   // .../pwmN
    std::string path_en;    // .../pwmN_enable
    std::string path_max;   // .../pwmN_max (optional)
    int max_raw{255};       // fallback
    bool writable{false};
  };

  struct HwmonFan {
    std::string hwmon;
    int index;              // fanN
    std::string path_input; // .../fanN_input
  };

  struct HwmonTemp {
    std::string hwmon;
    int index;              // tempN
    std::string path_input; // .../tempN_input
  };

  struct HwmonSnapshot {
    std::vector<HwmonPwm>  pwms;
    std::vector<HwmonFan>  fans;
    std::vector<HwmonTemp> temps;
  };

  class Hwmon {
  public:
    static HwmonSnapshot scan();
    static std::optional<int> readInt(const std::string& path);
    static bool writeInt(const std::string& path, int v);
    static bool setManual(const HwmonPwm& p);        // pwmN_enable -> 1
    static bool setAuto(const HwmonPwm& p);          // pwmN_enable -> 2 (if supported)
    static bool setPercent(const HwmonPwm& p, int pct); // 0..100 mapped to raw
    static std::optional<int> readRpm(const HwmonFan& f);
    static std::optional<int> readMilliC(const HwmonTemp& t);
  };

} // namespace lfc
