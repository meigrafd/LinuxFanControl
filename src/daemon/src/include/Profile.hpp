/*
 * Linux Fan Control — Profile (header)
 * - Configuration model for curves, controls and hwmon metadata
 * (c) 2025 LinuxFanControl contributors
 */
#pragma once

#include <string>
#include <vector>
#include <nlohmann/json.hpp>

#include "Hwmon.hpp"  // lfc::HwmonPwm/HwmonTemp

namespace lfc {

struct CurvePoint {
    double tempC {0.0};
    double percent {0.0};
};

enum class MixFunction {
    Avg = 1,
    Min = 0,
    Max = 2
};

struct FanCurveMeta {
    std::string name;
    std::string type;  // "graph" | "trigger" | "mix"

    // Graph: piecewise points
    std::vector<CurvePoint> points;

    // Trigger (new canonical fields; no onC/offC):
    double idleTemperature {0.0}; // replaces offC
    double loadTemperature {0.0}; // replaces onC
    double idleFanSpeed    {0.0}; // optional: % at idleTemperature
    double loadFanSpeed    {0.0}; // optional: % at loadTemperature

    // Sources/refs
    std::vector<std::string> tempSensors;
    std::vector<std::string> curveRefs;    // for mix
    std::vector<std::string> controlRefs;  // UI-only

    // Mix
    MixFunction mix {MixFunction::Avg};
};

struct ControlMeta {
    std::string name;
    std::string pwmPath;
    std::string curveRef;
    std::string nickName;
    bool enabled {true};
    bool hidden {false};
    bool manual {false};
    int  manualPercent {0};
};

struct HwmonDeviceMeta {
    std::string hwmonPath;
    std::string name;
    std::string vendor;
};

struct Profile {
    std::string schema;      // "lfc.profile/v1"
    std::string name;
    std::string description;
    std::string lfcdVersion;

    std::vector<FanCurveMeta> fanCurves;
    std::vector<ControlMeta>  controls;
    std::vector<HwmonDeviceMeta> hwmons;
};

// JSON (implemented in Profile.cpp)
void to_json(nlohmann::json& j, const CurvePoint& p);
void from_json(const nlohmann::json& j, CurvePoint& p);

void to_json(nlohmann::json& j, const FanCurveMeta& f);
void from_json(const nlohmann::json& j, FanCurveMeta& f);

void to_json(nlohmann::json& j, const ControlMeta& c);
void from_json(const nlohmann::json& j, ControlMeta& c);

void to_json(nlohmann::json& j, const HwmonDeviceMeta& d);
void from_json(const nlohmann::json& j, HwmonDeviceMeta& d);

void to_json(nlohmann::json& j, const Profile& p);
void from_json(const nlohmann::json& j, Profile& p);

// IO
Profile loadProfileFromFile(const std::string& path);
void    saveProfileToFile(const Profile& p, const std::string& path);

} // namespace lfc
