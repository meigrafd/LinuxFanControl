#pragma once
// (c) LinuxFanControl - Daemon hwmon backend (header)

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace lfc {

  struct TempSensor {
    std::string device;     // e.g. "hwmon4:amdgpu"
    std::string label;      // pretty label (may include type guess)
    std::string raw_label;  // original label from *_label or synthesized
    std::string type;       // "CPU","GPU","NVMe","Motherboard","Chipset","Water","Ambient","Unknown"
    std::string path;       // /sys/class/hwmon/..../tempX_input
    std::string unit;       // "°C"
  };

  struct PwmDevice {
    std::string device;         // e.g. "hwmon4:amdgpu"
    std::string label;          // "hwmon4:amdgpu:pwm1"
    std::string pwm_path;       // /sys/class/hwmon/..../pwmN
    std::string enable_path;    // /sys/class/hwmon/..../pwmN_enable (may be empty)
    std::string tach_path;      // /sys/class/hwmon/..../fanN_input (may be empty)
  };

  class Hwmon {
  public:
    // Enumerate hwmon tree
    static std::vector<TempSensor> DiscoverTemps(const std::string& base = "/sys/class/hwmon");
    static std::vector<PwmDevice>  DiscoverPwms (const std::string& base = "/sys/class/hwmon");

    // IO helpers
    static std::optional<double> ReadTempC(const std::string& temp_input_path);     // returns °C
    static std::optional<int>    ReadRpm  (const std::string& tach_path);
    static std::optional<int>    ReadPwmRaw(const std::string& pwm_path);           // 0..255
    static bool                  WritePwmRaw(const std::string& pwm_path, int raw); // 0..255
    static bool                  WritePwmPercent(const std::string& pwm_path, double percent); // 0..100
    static std::optional<std::string> ReadText(const std::string& path);
    static bool                       WriteText(const std::string& path, std::string_view text);

    // enable= "1" manual, "2" automatic (convention varies per driver); we write exactly what you request
    static std::optional<std::string> ReadEnable(const std::string& enable_path);
    static bool                       WriteEnable(const std::string& enable_path, std::string_view v);

    // FS capabilities
    static bool Exists(const std::string& path);
    static bool IsWritable(const std::string& path);

    // Small helpers
    static double MilliToC(double v);
  };

} // namespace lfc
