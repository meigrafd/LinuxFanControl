/*
 * Linux Fan Control — Hwmon sysfs snapshot (header)
 * - Collects temperature, fan tachometer and PWM nodes
 * - Normalizes all sysfs paths to avoid ".." segments
 * (c) 2025 LinuxFanControl contributors
 */
#pragma once

#include <string>
#include <vector>
#include <optional>

namespace lfc {

struct HwmonTemp {
    std::string path_input;   // normalized absolute path to temp*_input
    std::string label;        // optional label from temp*_label
};

struct HwmonFan {
    std::string path_input;   // normalized absolute path to fan*_input
};

struct HwmonPwm {
    std::string path_pwm;     // normalized absolute path to pwm*_file
    std::string path_enable;  // normalized absolute path to pwm*_enable
};

struct HwmonSnapshot {
    std::vector<HwmonTemp> temps;
    std::vector<HwmonFan> fans;
    std::vector<HwmonPwm> pwms;
};

namespace Hwmon {

// Scan sysfs (and optionally libsensors if enabled) and return snapshot.
// All paths returned are normalized.
HwmonSnapshot scan(const std::string& root = "/sys/class/hwmon");

// Reading helpers. Return std::nullopt on failure.
std::optional<int> readMilliC(const HwmonTemp& t);      // milli-°C
std::optional<double> readTempC(const HwmonTemp& t);    // °C (double)
std::optional<int> readRpm(const HwmonFan& f);          // RPM
std::optional<int> readPercent(const HwmonPwm& p);      // 0..100

// Write helpers. No-ops on failure.
void setManual(const HwmonPwm& p);
void setAuto(const HwmonPwm& p);
void setPercent(const HwmonPwm& p, int pct);

} // namespace Hwmon
} // namespace lfc
