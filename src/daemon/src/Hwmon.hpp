/*
 * Linux Fan Control — HWMON abstraction (header)
 * - Sysfs discovery (temps, fans, pwms)
 * - Optional libsensors discovery (if built)
 * - Minimal I/O helpers for engine
 * (c) 2025 LinuxFanControl contributors
 */
#pragma once
#include <string>
#include <vector>
#include <optional>
#include <algorithm>

namespace lfc {

// Basic sensor descriptors
struct TempSensor {
    std::string hwmon;       // e.g. "/sys/class/hwmon/hwmon3" or "libsensors"
    int index{0};            // sysfs index; for libsensors an incremental index
    std::string path_input;  // sysfs: full path to temp*_input; libsensors: "sensors:<label>"
    std::string label;       // user-friendly label if available
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
    // Generic file I/O helpers
    static std::optional<int> readInt(const std::string& path);
    static bool writeInt(const std::string& path, int value);

    // Temperatures
    // - readMilliC: temperature in milli-Celsius (mC), backend-agnostic
    // - readTempC : integer °C (legacy helper, derived from mC)
    // - readTempC2dp: precise °C with two decimals (for RPC/GUI display)
    static std::optional<int>    readMilliC(const TempSensor& t);
    static std::optional<int>    readTempC(const TempSensor& t);
    static std::optional<double> readTempC2dp(const TempSensor& t);

    // Fans / PWM
    static std::optional<int> readRpm(const HwmonFan& f);
    static std::optional<int> readPercent(const HwmonPwm& p);
    static void setManual(const HwmonPwm& p);
    static void setPercent(const HwmonPwm& p, int percent); // 0..100

    // Discovery (sysfs always; libsensors added if available)
    static HwmonSnapshot scan();
};

} // namespace lfc
