#pragma once
// Thin hwmon helpers + discovery (comments in English)

#include <string>
#include <vector>
#include <limits>

struct TempSensorInfo {
    std::string name;   // short display name (e.g. "temp1" / label)
    std::string label;  // nicer composite if available (device:label)
    std::string path;   // /sys/class/hwmon/.../tempN_input
    std::string type;   // CPU/GPU/Water/Ambient/Unknown (best-effort)
};

// Keep Daemon backward-compatible: it expects PwmOutputInfo
struct PwmDevice {
    std::string pwm_path;    // /sys/.../pwmN
    std::string enable_path; // /sys/.../pwmN_enable (optional)
    std::string tach_path;   // /sys/.../fanN_input (optional)
    double min_pct{0.0};
    double max_pct{100.0};
};
using PwmOutputInfo = PwmDevice; // alias for Daemon.h expectations

class Hwmon {
public:
    std::vector<TempSensorInfo> discoverTemps() const;
    std::vector<PwmDevice>      discoverPwms()  const;

    // Static helpers used by Engine/Daemon
    static double readTempC(const std::string& path);                 // °C (milli-degree aware)
    static bool   setPwmPercent(const PwmDevice& dev, double pct,     // write 0..100 % (enables manual)
    std::string* err = nullptr);

private:
    static double milli_to_c(double v) { return (v > 200.0 ? v/1000.0 : v); }
};
