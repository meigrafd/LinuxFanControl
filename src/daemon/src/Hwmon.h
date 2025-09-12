/*
 * Linux Fan Control (LFC) - hwmon interfaces
 * (c) 2025 meigrafd & contributors - MIT License (see LICENSE)
 */

#pragma once
#include <string>
#include <vector>

// Simple temperature sensor descriptor discovered via /sys/class/hwmon
struct TempSensorInfo {
    std::string name;   // hwmon device name (e.g., "k10temp", "amdgpu", ...)
    std::string label;  // label for tempN (or "tempN" if no label file)
    std::string path;   // full path to tempN_input
};

// Simple PWM device descriptor discovered via /sys/class/hwmon
struct PwmDevice {
    std::string label;       // human-readable id, e.g. "hwmon4:amdgpu:pwm1"
    std::string pwm_path;    // /sys/class/hwmon/.../pwmN
    std::string enable_path; // /sys/class/hwmon/.../pwmN_enable (may be empty)
    std::string tach_path;   // /sys/class/hwmon/.../fanN_input (may be empty)
};

class Hwmon {
public:
    // Enumerate temperature sensors and pwm devices from /sys/class/hwmon
    std::vector<TempSensorInfo> discoverTemps() const;
    std::vector<PwmDevice>      discoverPwms() const;

    // Helpers
    static double readTempC(const std::string& path); // returns °C or NaN
    static bool   setPwmPercent(const PwmDevice& dev, double percent, std::string* err);
};
