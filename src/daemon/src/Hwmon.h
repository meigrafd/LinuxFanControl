#pragma once
#include <string>
#include <vector>
#include <optional>

/* Sysfs hwmon helpers: enumerate temps & pwms, read temps, write pwm safely. */

struct TempSensor {
    std::string label;   // ui label: "hwmonX:name:tempN_label" or synthesized
    std::string path;    // /sys/class/hwmon/.../tempN_input
    std::string type;    // heuristic (CPU/GPU/NVMe/...)
};

struct PwmDevice {
    std::string label;        // "hwmonX:name:pwmN"
    std::string pwm_path;     // /sys/.../pwmN
    std::string enable_path;  // /sys/.../pwmN_enable (optional)
    std::string tach_path;    // /sys/.../fanN_input (optional)
    bool writable = false;
};

namespace Hwmon {

    std::vector<TempSensor> discoverTemps(const std::string& base="/sys/class/hwmon");
    std::vector<PwmDevice>  discoverPwms(const std::string& base="/sys/class/hwmon");

    std::optional<double> readTempC(const std::string& path);
    std::optional<int>    readRpm(const std::string& path);

    bool readText(const std::string& path, std::string& out);
    bool writeText(const std::string& path, const std::string& text);

    bool setPwmPercent(const PwmDevice& dev, double pct, std::string* err=nullptr);
    double clamp(double v, double lo, double hi);

    std::string classify(const std::string& hwmon_name, const std::string& label); // rough heuristic

} // namespace Hwmon
