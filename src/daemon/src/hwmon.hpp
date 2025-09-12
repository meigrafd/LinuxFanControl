#pragma once
#include <string>
#include <vector>
#include <optional>

struct TempSensor {
  std::string label;     // "hwmonX:chip:CPU Tctl"
  std::string path;      // /sys/class/hwmon/hwmonX/tempN_input
  std::string type;      // CPU/GPU/...
  std::string unit;      // "°C"
};

struct PwmDevice {
  std::string label;         // "hwmonX:chip:pwmN"
  std::string pwm_path;      // /sys/class/hwmon/hwmonX/pwmN
  std::optional<std::string> enable_path; // .../pwmN_enable
  std::optional<std::string> tach_path;   // .../fanN_input
  bool writable = false;     // probed
};

std::vector<TempSensor> enumerate_temps();
std::vector<PwmDevice>  enumerate_pwms();

double read_temp_c(const std::string& path);  // throws on error
int    read_rpm(const std::string& path);     // throws on error
bool   set_pwm_enable(const std::string& enable_path, int mode); // best-effort
bool   write_pwm_pct(const std::string& pwm_path, double pct);   // 0..100, best-effort

// probe if pwm is writable (try enable=1, no-op write & restore)
bool   probe_pwm_writable(PwmDevice &dev, std::string &reason);
