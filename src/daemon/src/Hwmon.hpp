#pragma once
/*
 * /sys/class/hwmon scanner & helpers
 * (c) 2025 LinuxFanControl contributors
 */
#include <string>
#include <vector>
#include <optional>

struct HwmonSensor {
  std::string id;        // e.g. "hwmon4:temp1"
  std::string label;
  std::string type;      // "temp" | "fan"
  std::string path;      // full file path (temp*_input or fan*_input)
};

struct HwmonPwm {
  std::string id;          // e.g. "hwmon4:pwm1"
  std::string label;       // name or chip+index
  std::string pathPwm;     // pwmN
  std::string pathEnable;  // pwmN_enable if present
  int minDuty = 0;         // [0..255] raw
  int maxDuty = 255;
  bool writable = false;
};

struct HwmonSnapshot {
  std::vector<HwmonSensor> sensors;
  std::vector<HwmonPwm>    pwms;
};

namespace hwmon {

  HwmonSnapshot scanSysfs(const std::string& root="/sys/class/hwmon");

  // returns value in engineering units (temp °C, fan RPM)
  std::optional<double> readValue(const HwmonSensor& s);

  // raw 0..255 write, returns false on error
  bool writePwmRaw(const HwmonPwm& p, int value);

  // try to switch to manual mode (pwmX_enable=1), returns whether manual is available
  bool enableManual(const HwmonPwm& p);

  // read current pwm raw 0..255 (if readable)
  std::optional<int> readPwmRaw(const HwmonPwm& p);

}
