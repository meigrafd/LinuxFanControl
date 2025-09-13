#pragma once
#include <string>
#include <vector>
#include <optional>

struct TempSensor {
  std::string device, label, path, unit="°C", type="Unknown";
};
struct PwmOutput {
  std::string device, label, pwm_path, enable_path, tach_path;
};
struct CalibResult {
  bool ok{false}; int spinup_pct{0}; int min_pct{0}; int rpm_at_min{0};
  bool aborted{false}; std::string error;
};

class Hwmon {
public:
  static std::vector<TempSensor> discoverTemps(const std::string& base="/sys/class/hwmon");
  static std::vector<PwmOutput>  discoverPwms(const std::string& base="/sys/class/hwmon");
  static std::optional<double>   readTempC(const std::string& path);
  static std::optional<int>      readRpm(const std::string& path);
  static std::optional<int>      readPwmRaw(const std::string& path);
  static bool                    writePwmPct(const std::string& path, int pct, int floor_pct=20);
  static bool                    writeRaw(const std::string& path, const std::string& val);
  static std::string             readText(const std::string& path);
  static CalibResult             calibrate(const PwmOutput& p, int start=0, int end=100, int step=5, double settle_s=1.0, int floor_pct=20, int rpm_threshold=100, bool (*cancelled)()=nullptr);
};
