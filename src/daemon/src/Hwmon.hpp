/*
 * Linux Fan Control — HWMON abstraction (header)
 * - Sysfs discovery (temps, fans, pwms)
 * - Minimal I/O helpers for engine
 * - Percent-based PWM control
 * (c) 2025 LinuxFanControl contributors
 */
#pragma once
#include <string>
#include <vector>
#include <optional>

namespace lfc {

  // Basic sensor descriptors
  struct TempSensor {
    std::string hwmon;
    int index{0};
    std::string path_input;
    std::string label;
  };

  struct PwmDevice {
    std::string hwmon;
    int index{0};
    std::string path_pwm;
    std::string path_enable;
  };

  using HwmonPwm = PwmDevice;

  struct HwmonFan {
    std::string hwmon;
    int index{0};
    std::string path_input;
  };

  struct HwmonSnapshot {
    std::vector<HwmonPwm> pwms;
    std::vector<HwmonFan> fans;
    std::vector<TempSensor> temps;
  };

  // Static helpers used by the engine/daemon
  class Hwmon {
  public:
    static std::optional<int> readInt(const std::string& path);
    static bool writeInt(const std::string& path, int value);

    static std::optional<int> readMilliC(const TempSensor& t);
    static std::optional<int> readTempC(const TempSensor& t);
    static std::optional<int> readRpm(const HwmonFan& f);

    static void setManual(const HwmonPwm& p);
    static void setPercent(const HwmonPwm& p, int percent); // 0..100

    static HwmonSnapshot scan();
  };

} // namespace lfc
