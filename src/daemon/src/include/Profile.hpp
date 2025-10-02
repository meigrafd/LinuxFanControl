/*
 * Linux Fan Control — Profile (header)
 * - Configuration model for curves, controls and hwmon metadata
 * (c) 2025 LinuxFanControl contributors
 */
#pragma once

#include <string>
#include <vector>
#include <nlohmann/json.hpp>

#include "Hwmon.hpp"  // for lfc::HwmonPwm/HwmonTemp paths

namespace lfc {

struct CurvePoint {
    double tempC{0.0};
    double percent{0.0};
};

enum class MixFunction {
    Min,
    Avg,
    Max
};

struct FanCurveMeta {
    std::string name;
    std::string type;                      // "graph", "trigger", "mix" (optional info)
    MixFunction  mix{MixFunction::Avg};    // only for type=="mix"
    std::vector<std::string> tempSensors;  // identifiers/paths of temperature sources
    std::vector<std::string> curveRefs;    // referenced curve names (mix)
    std::vector<std::string> controlRefs;  // reverse refs (filled elsewhere)
    std::vector<CurvePoint>  points;       // normalized 0..100% curve
    double onC{0.0};                       // optional trigger ON threshold (if imported as trigger)
    double offC{0.0};                      // optional trigger OFF threshold (if imported as trigger)
};

struct ControlMeta {
    std::string name;          // system-assigned stable name ("Fan #1", …)
    std::string pwmPath;       // system path or imported identifier
    std::string curveRef;      // name of selected curve (if any)
    std::string nickName;      // user label (GUI)
    bool        enabled{true}; // GUI: Enable (FanControl "Enable")
    bool        hidden{false}; // GUI: IsHidden  (FanControl "IsHidden")
    bool        manual{false}; // ManualControl
    int      manualPercent{0}; // ManualControlValue in percent
};

struct HwmonDeviceMeta {
    std::string hwmonPath;
    std::string name;
    std::string vendor;
    std::vector<HwmonPwm> pwms;
};

struct Profile {
    std::string schema;         // optional schema/format tag
    std::string name;           // profile name
    std::string description;    // optional
    std::string lfcdVersion;    // producer version
    std::vector<FanCurveMeta>  fanCurves;
    std::vector<ControlMeta>   controls;
    std::vector<HwmonDeviceMeta> hwmons;
};

// JSON declarations (implemented in Profile.cpp)
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

// helpers (implemented in Profile.cpp)
Profile loadProfileFromFile(const std::string& path);
void    saveProfileToFile(const Profile& p, const std::string& path);

} // namespace lfc
