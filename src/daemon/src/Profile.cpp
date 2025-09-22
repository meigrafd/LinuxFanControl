/*
 * Linux Fan Control — Profile (implementation)
 * (c) 2025 LinuxFanControl contributors
 */
#include "include/Profile.hpp"
#include "include/Version.hpp"

#include <fstream>
#include <filesystem>

namespace fs = std::filesystem;
using nlohmann::json;

namespace lfc {

/* ------------------ CurvePoint ------------------ */

void to_json(json& j, const CurvePoint& p) {
    j = json{
        {"tempC",   p.tempC},
        {"percent", p.percent}
    };
}

void from_json(const json& j, CurvePoint& p) {
    p.tempC   = j.value("tempC", 0.0);
    p.percent = j.value("percent", 0);
}

/* ------------------ FanCurveMeta ------------------ */

static std::string mixToString(MixFunction m) {
    switch (m) {
        case MixFunction::Min: return "min";
        case MixFunction::Avg: return "avg";
        case MixFunction::Max: return "max";
    }
    return "avg";
}

static MixFunction mixFromString(const std::string& s) {
    if (s == "min") return MixFunction::Min;
    if (s == "max") return MixFunction::Max;
    return MixFunction::Avg;
}

void to_json(json& j, const FanCurveMeta& f) {
    j = json{
        {"name",        f.name},
        {"type",        f.type},
        {"mix",         mixToString(f.mix)},
        {"tempSensors", f.tempSensors},
        {"curveRefs",   f.curveRefs},
        {"controlRefs", f.controlRefs},
        {"points",      f.points},
        {"onC",         f.onC},
        {"offC",        f.offC}
    };
}

void from_json(const json& j, FanCurveMeta& f) {
    f.name = j.value("name", std::string{});
    f.type = j.value("type", std::string{"graph"});
    f.mix  = mixFromString(j.value("mix", std::string{"avg"}));

    f.tempSensors = j.value("tempSensors", std::vector<std::string>{});
    f.curveRefs   = j.value("curveRefs",   std::vector<std::string>{});
    f.controlRefs = j.value("controlRefs", std::vector<std::string>{});

    f.points.clear();
    if (j.contains("points") && j.at("points").is_array()) {
        for (const auto& it : j.at("points")) {
            f.points.push_back(it.get<CurvePoint>());
        }
    }

    f.onC  = j.value("onC",  0.0);
    f.offC = j.value("offC", 0.0);
}

/* ------------------ ControlMeta ------------------ */

void to_json(json& j, const ControlMeta& c) {
    j = json{
        {"name",     c.name},
        {"pwmPath",  c.pwmPath},
        {"curveRef", c.curveRef},
        {"nickName", c.nickName}
    };
}

void from_json(const json& j, ControlMeta& c) {
    c.name     = j.value("name", std::string{});
    c.pwmPath  = j.value("pwmPath", std::string{});
    c.curveRef = j.value("curveRef", std::string{});
    if (j.contains("nickName"))      c.nickName = j.at("nickName").get<std::string>();
    else if (j.contains("nick"))     c.nickName = j.at("nick").get<std::string>();
    else if (j.contains("nickname")) c.nickName = j.at("nickname").get<std::string>();
}

/* ------------------ HwmonDeviceMeta ------------------ */

void to_json(json& j, const HwmonDeviceMeta& d) {
    j = json{
        {"hwmonPath", d.hwmonPath},
        {"name",      d.name},
        {"vendor",    d.vendor}
    };
    // 'pwms' omitted intentionally (runtime inventory provides pwm metadata)
}

void from_json(const json& j, HwmonDeviceMeta& d) {
    d.hwmonPath = j.value("hwmonPath", std::string{});
    d.name      = j.value("name", std::string{});
    d.vendor    = j.value("vendor", std::string{});
    // do not touch 'pwms' here; it is runtime-filled elsewhere
}

/* ------------------ Profile ------------------ */

void to_json(json& j, const Profile& p) {
    j = json{
        {"schema",      p.schema},
        {"name",        p.name},
        {"description", p.description},
        {"lfcdVersion", p.lfcdVersion},
        {"fanCurves",   p.fanCurves},
        {"controls",    p.controls},
        {"hwmons",      p.hwmons}
    };
}

void from_json(const json& j, Profile& p) {
    p.schema      = j.value("schema",      std::string{"lfc.profile/v1"});
    p.name        = j.value("name",        std::string{});
    p.description = j.value("description", std::string{});
    p.lfcdVersion = j.value("lfcdVersion", std::string{LFCD_VERSION});

    p.fanCurves.clear();
    if (j.contains("fanCurves") && j.at("fanCurves").is_array()) {
        for (const auto& it : j.at("fanCurves")) p.fanCurves.push_back(it.get<FanCurveMeta>());
    }

    p.controls.clear();
    if (j.contains("controls") && j.at("controls").is_array()) {
        for (const auto& it : j.at("controls")) p.controls.push_back(it.get<ControlMeta>());
    }

    p.hwmons.clear();
    if (j.contains("hwmons") && j.at("hwmons").is_array()) {
        for (const auto& it : j.at("hwmons")) p.hwmons.push_back(it.get<HwmonDeviceMeta>());
    }
}

Profile loadProfileFromFile(const std::string& path) {
    std::ifstream f(path);
    json j;
    f >> j;
    return j.get<Profile>();
}

void saveProfileToFile(const Profile& p, const std::string& path) {
    json j = p;
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out << j.dump(2) << "\n";
}

} // namespace lfc
