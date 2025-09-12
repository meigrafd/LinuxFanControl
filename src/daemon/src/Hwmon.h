#pragma once
#include <string>
#include <vector>
#include <limits>

struct TempSensorInfo {
    std::string name;
    std::string label;
    std::string path;
    std::string type;
};

struct PwmDevice {
    std::string pwm_path;
    std::string enable_path;
    std::string tach_path;
    double min_pct{0.0};
    double max_pct{100.0};
};
using PwmOutputInfo = PwmDevice; // Daemon compatibility

class Hwmon {
public:
    std::vector<TempSensorInfo> discoverTemps() const;
    std::vector<PwmDevice>      discoverPwms()  const;

    static double readTempC(const std::string& path);
    static bool   setPwmPercent(const PwmDevice& dev, double pct, std::string* err=nullptr);

private:
    static double milli_to_c(double v) { return (v > 200.0 ? v/1000.0 : v); }
};
