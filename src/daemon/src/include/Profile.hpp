/*
 * Linux Fan Control — Profile (header)
 * - Configuration model for curves, controls and hwmon metadata
 * (c) 2025 LinuxFanControl contributors
 */
#pragma once

#include <string>
#include <vector>
#include <nlohmann/json.hpp>

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
    std::vector<std::string> tempSensors;
    std::vector<CurvePoint> points;
    double onC{0.0};
    double offC{0.0};
};

struct ControlMeta {
    std::string name;
    std::string pwmPath;
    std::string curveRef;
};

struct HwmonPwm; // forward

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
struct Profile;
Profile loadProfileFromFile(const std::string& path);
void    saveProfileToFile(const Profile& p, const std::string& path);

} // namespace lfc
