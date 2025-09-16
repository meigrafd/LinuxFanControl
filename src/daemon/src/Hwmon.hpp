/*
 * Linux Fan Control — Hwmon interface (header)
 * Provides lightweight accessors for /sys/class/hwmon
 * (c) 2025 LinuxFanControl contributors
 */
#pragma once

#include <string>
#include <vector>
#include <optional>

namespace lfc {

// Simple value objects that carry absolute sysfs paths
struct HwmonTemp {
    std::string path_input;   // e.g. /sys/class/hwmon/hwmonX/temp1_input
    std::string label;        // optional friendly name
};

struct HwmonFan {
    std::string path_input;   // e.g. /sys/class/hwmon/hwmonX/fan1_input
};

struct HwmonPwm {
    std::string path_pwm;     // e.g. /sys/class/hwmon/hwmonX/pwm1
    std::string path_enable;  // e.g. /sys/class/hwmon/hwmonX/pwm1_enable
};

struct HwmonInventory {
    std::vector<HwmonTemp> temps;
    std::vector<HwmonFan>  fans;
    std::vector<HwmonPwm>  pwms;
};

class Hwmon {
public:
    // inventory
    static HwmonInventory scan();

    // readers
    static std::optional<double> readTempC(const HwmonTemp& t); // °C
    static std::optional<int>    readRpm(const HwmonFan& f);
    static std::optional<int>    readPercent(const HwmonPwm& p); // 0..100 if available
    static std::optional<int>    readRaw(const HwmonPwm& p);     // raw 0..255

    // writers (manual control expects pwm*_enable=1)
    static void setPercent(const HwmonPwm& p, int percent);      // clamps 0..100
    static void writeRawPath(const std::string& path_pwm, int raw); // raw 0..255

    // enable helpers for restoring BIOS/driver control
    static std::optional<int> readEnable(const HwmonPwm& p);                    // returns 1/2/0
    static void               writeEnable(const std::string& path_enable, int); // write mode
    static void               writeEnable(const HwmonPwm& p, int mode);         // overload convenience

    // utility
    static std::string percentToString(int v);
};

} // namespace lfc
