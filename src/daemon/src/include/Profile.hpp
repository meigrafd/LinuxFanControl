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
    int percent{0};
};

enum class MixFunction {
    Min,
    Avg,
    Max
};

struct FanCurveMeta {
    std::string name;
    std::string type;                  // "graph", "trigger", "mix"
    MixFunction mix{MixFunction::Avg};
    std::vector<std::string> tempSensors;     // sysfs temp input paths used by this curve (graphs/triggers)
    std::vector<std::string> curveRefs;       // referenced curve names (for type=="mix")
    std::vector<std::string> controlRefs;     // reverse refs: controls using this curve (from Controls[].SelectedFanCurve)
    std::vector<CurvePoint> points;           // normalized 0..100% curve
    double onC{0.0};                           // optional trigger ON threshold for imported trigger-style curves
    double offC{0.0};                          // optional trigger OFF threshold for imported trigger-style curves
};

struct ControlMeta {
    std::string name;
    std::string pwmPath;
    std::string curveRef;
    std::string nickName;   // imported from FanControl.Release "NickName"
};

struct HwmonDeviceMeta {
    std::string hwmonPath;
    std::string name;
    std::string vendor;
    std::vector<HwmonPwm> pwms;
};

struct Profile {
    std::string schema;
    std::string name;
    std::string description;
    std::string lfcdVersion;
    std::vector<FanCurveMeta> fanCurves;
    std::vector<ControlMeta> controls;
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
