/*
 * Linux Fan Control — Hwmon backends (header)
 * - Sysfs scanner (read/write)
 * - Optional libsensors reader (read-only) if compiled
 * - All sysfs paths are normalized to avoid ".." segments
 * (c) 2025 LinuxFanControl contributors
 */
#pragma once

#include <string>
#include <vector>
#include <optional>

namespace lfc {

enum class HwBackend {
    Auto,       // prefer sysfs (writes supported); if empty, try libsensors
    Sysfs,      // force sysfs only
    Libsensors  // force libsensors (read-only temps/fans)
};

struct HwmonTemp {
    std::string path_input;   // normalized absolute sysfs path (sysfs) or pseudo "libsensors:<chip>:<name>"
    std::string label;        // display label (from temp*_label or libsensors label)
};

struct HwmonFan {
    std::string path_input;   // normalized absolute sysfs path or pseudo "libsensors:<chip>:<name>"
};

struct HwmonPwm {
    std::string path_pwm;     // normalized absolute sysfs path
    std::string path_enable;  // normalized absolute sysfs path
};

struct HwmonSnapshot {
    std::vector<HwmonTemp> temps;
    std::vector<HwmonFan>  fans;
    std::vector<HwmonPwm>  pwms;
    HwBackend              backend{HwBackend::Sysfs};
};

namespace Hwmon {

// Scan using selected backend. For sysfs, 'root' is the hwmon class directory.
HwmonSnapshot scan(HwBackend backend = HwBackend::Auto, const std::string& root = "/sys/class/hwmon");

// Reading helpers. Return std::nullopt on failure.
std::optional<int>    readMilliC(const HwmonTemp& t);   // milli-°C
std::optional<double> readTempC(const HwmonTemp& t);    // °C (double)
std::optional<int>    readRpm(const HwmonFan& f);       // RPM
std::optional<int>    readPercent(const HwmonPwm& p);   // 0..100 (sysfs)

// Write helpers (sysfs only; no-ops for libsensors pseudo-nodes).
void setManual(const HwmonPwm& p);
void setAuto(const HwmonPwm& p);
void setPercent(const HwmonPwm& p, int pct);

} // namespace Hwmon

} // namespace lfc
