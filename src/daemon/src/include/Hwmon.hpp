/*
 * Linux Fan Control — Hwmon interface
 * (c) 2025 LinuxFanControl contributors
 */
#pragma once

#include <optional>
#include <string>
#include <vector>

namespace lfc {

struct HwmonChip {
    std::string hwmonPath;
    std::string name;
    std::string vendor;
};

struct HwmonTemp {
    std::string chipPath;
    std::string path_input;
    std::string label;
};

struct HwmonFan {
    std::string chipPath;
    std::string path_input;
    std::string label;
};

struct HwmonPwm {
    std::string chipPath;
    std::string path_pwm;
    std::string path_enable;
    int pwm_max{255};
    std::string label;
};

struct HwmonInventory {
    std::vector<HwmonChip> chips;
    std::vector<HwmonTemp> temps;
    std::vector<HwmonFan>  fans;
    std::vector<HwmonPwm>  pwms;
};

/* Snapshot = current inventory state for telemetry */
using HwmonSnapshot = HwmonInventory;

class Hwmon {
public:
    // Full scan of /sys/class/hwmon (hardware discovery)
    static HwmonInventory scan();

    // Lightweight refresh: update values/metadata for an existing snapshot,
    // drop entries whose backing files vanished; DO NOT discover new hardware.
    static void refreshValues(HwmonInventory& s);

    // Reading helpers
    static std::optional<double> readTempC(const HwmonTemp& t);
    static std::optional<int>    readRpm(const HwmonFan& f);
    static std::optional<int>    readPercent(const HwmonPwm& p);
    static std::optional<int>    readRaw(const HwmonPwm& p);
    static std::optional<int>    readEnable(const HwmonPwm& p);

    // Writing helpers (high-level & raw)
    static bool setPercent(const HwmonPwm& p, int percent);
    static bool setRaw(const HwmonPwm& p, int raw);
    static bool setEnable(const HwmonPwm& p, int mode);

    static bool writeRaw(const std::string& path,   int raw);
    static bool writeEnable(const std::string& path, int mode);

    // Metadata helpers
    static std::string chipNameForPath(const std::string& chipPath);
    static std::string chipVendorForName(const std::string& chipName);
};

} // namespace lfc
