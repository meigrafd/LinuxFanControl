#pragma once
// Thin hwmon helpers + discovery
// Comments in English per project preference.

#include <string>
#include <vector>
#include <limits>

struct TempSensorInfo {
    std::string label;
    std::string path;
    std::string type; // optional classification
};

struct PwmDevice {
    std::string pwm_path;
    std::string enable_path;
    std::string tach_path;
    double min_pct{0.0};
    double max_pct{100.0};
};

class Hwmon {
public:
    std::vector<TempSensorInfo> discoverTemps() const;
    std::vector<PwmDevice>      discoverPwms()  const;

    // Static helpers (used by Engine & daemon):
    // Read °C (hwmon often provides millidegree values)
    static double readTempC(const std::string& path);
    // Write PWM percent [0..100], enabling manual mode if possible
    static bool   setPwmPercent(const PwmDevice& dev, double pct, std::string* err=nullptr);

private:
    static double milli_to_c(double v) { return (v > 200.0 ? v/1000.0 : v); }
};
