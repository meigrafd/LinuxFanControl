/*
 * Linux Fan Control — Profile (implementation)
 * (c) 2025 LinuxFanControl contributors
 *
 * Notes:
 *  - Preserve curve type faithfully across save/load.
 *  - Write only fields that semantically belong to the type:
 *      graph   → points, tempSensors
 *      trigger → IdleTemperature, LoadTemperature, IdleFanSpeed, LoadFanSpeed, tempSensors
 *      mix     → curveRefs, mix
 *  - Robust load fallback: if type looks inconsistent (e.g. type=graph but no
 *    points and ≥2 curveRefs), coerce to mix; if type=trigger but thresholds
 *    are zero and points exist, coerce to graph.
 */

#include "include/Profile.hpp"
#include "include/Version.hpp"   // LFCD_VERSION
#include "include/Utils.hpp"     // util::read_json_file, util::write_text_file
#include "include/Log.hpp"

#include <nlohmann/json.hpp>
#include <algorithm>
#include <fstream>
#include <string>
#include <vector>
#include <stdexcept>

namespace lfc {

using nlohmann::json;

/* ============================ helpers ============================ */

static std::string mixToStr(MixFunction m) {
    switch (m) {
        case MixFunction::Min: return "min";
        case MixFunction::Max: return "max";
        case MixFunction::Avg:
        default: return "avg";
    }
}
static MixFunction mixFromAny(const json& v, MixFunction def = MixFunction::Avg) {
    if (v.is_string()) {
        const auto s = v.get<std::string>();
        if (s == "min") return MixFunction::Min;
        if (s == "max") return MixFunction::Max;
        if (s == "avg") return MixFunction::Avg;
    } else if (v.is_number_integer()) {
        const int n = (int)v.get<long long>();
        if (n == 0) return MixFunction::Min;
        if (n == 1) return MixFunction::Max;
        if (n == 2) return MixFunction::Avg;
    }
    return def;
}

/* ========================= CurvePoint ========================= */

void to_json(json& j, const CurvePoint& p) {
    j = json{{"tempC", p.tempC}, {"percent", p.percent}};
}
void from_json(const json& j, CurvePoint& p) {
    p.tempC   = j.value("tempC", 0.0);
    p.percent = j.value("percent", 0.0);
}

/* ======================== FanCurveMeta ======================== */

void to_json(json& j, const FanCurveMeta& f) {
    // Always write name/type, then type-specific fields to avoid ambiguity.
    j = json{
        {"name", f.name},
        {"type", f.type}
    };

    if (f.type == "graph") {
        j["points"]      = f.points;
        j["tempSensors"] = f.tempSensors;   // one per curve in our pipeline
    } else if (f.type == "trigger") {
        // New trigger schema: FCR-style names only (no onC/offC)
        j["IdleTemperature"] = f.idleTemperature;
        j["LoadTemperature"] = f.loadTemperature;
        j["IdleFanSpeed"]    = f.idleFanSpeed;
        j["LoadFanSpeed"]    = f.loadFanSpeed;
        j["tempSensors"]     = f.tempSensors;
    } else if (f.type == "mix") {
        j["mix"]       = mixToStr(f.mix);
        j["curveRefs"] = f.curveRefs;
    } else {
        // Unknown type: store everything we have to avoid data loss.
        j["points"]        = f.points;
        j["tempSensors"]   = f.tempSensors;
        j["IdleTemperature"] = f.idleTemperature;
        j["LoadTemperature"] = f.loadTemperature;
        j["IdleFanSpeed"]    = f.idleFanSpeed;
        j["LoadFanSpeed"]    = f.loadFanSpeed;
        j["mix"]           = mixToStr(f.mix);
        j["curveRefs"]     = f.curveRefs;
    }

    // controlRefs are meta links (UI), safe to keep across types
    if (!f.controlRefs.empty()) j["controlRefs"] = f.controlRefs;
}

void from_json(const json& j, FanCurveMeta& f) {f = FanCurveMeta{}; // reset
f.name = j.value("name", std::string{});
f.type = j.value("type", std::string{});

// Load everything that might be present.
if (j.contains("points"))       f.points      = j.at("points").get<std::vector<CurvePoint>>();
if (j.contains("tempSensors"))  f.tempSensors = j.at("tempSensors").get<std::vector<std::string>>();
if (j.contains("curveRefs"))    f.curveRefs   = j.at("curveRefs").get<std::vector<std::string>>();
if (j.contains("controlRefs"))  f.controlRefs = j.at("controlRefs").get<std::vector<std::string>>();
f.mix  = mixFromAny(j.value("mix", json{}), MixFunction::Avg);

// New trigger schema (no back-compat): read FCR-style names only
f.idleTemperature = j.value("IdleTemperature", 0.0);
f.loadTemperature = j.value("LoadTemperature", 0.0);
f.idleFanSpeed    = j.value("IdleFanSpeed", 0.0);
f.loadFanSpeed    = j.value("LoadFanSpeed", 0.0);

// Robust type reconciliation in case of inconsistent data:
const bool hasPoints   = !f.points.empty();
const bool hasRefsMix  = f.curveRefs.size() >= 2;
const bool hasThresh   = (f.loadTemperature != 0.0 || f.idleTemperature != 0.0);

if (f.type == "mix") {
    // Ensure we don't carry meaningless data for mix
    f.points.clear();
    f.tempSensors.clear(); // sensors come from referenced curves
    // trigger fields are irrelevant for mix
    f.idleTemperature = f.loadTemperature = 0.0;
    f.idleFanSpeed = f.loadFanSpeed = 0.0;
} else if (f.type == "trigger") {
    // Trigger should not have points (we keep speeds/thresholds only)
    f.points.clear();
    f.curveRefs.clear();
} else if (f.type == "graph") {
    // Graph should not carry trigger thresholds or curveRefs
    f.idleTemperature = f.loadTemperature = 0.0;
    f.idleFanSpeed = f.loadFanSpeed = 0.0;
    f.curveRefs.clear();
}

// Fallbacks: fix obviously wrong combinations
if (f.type.empty()) {
    // Deduce type from content
    if (hasRefsMix)       f.type = "mix";
    else if (hasThresh)   f.type = "trigger";
    else                  f.type = "graph";
} else if (f.type == "graph" && !hasPoints && hasRefsMix) {
    // Looks like a mix that was mis-labeled as graph
    f.type = "mix";
    f.tempSensors.clear();
    f.idleTemperature = f.loadTemperature = 0.0;
    f.idleFanSpeed = f.loadFanSpeed = 0.0;
} else if (f.type == "trigger" && hasPoints && !hasThresh) {
    // Looks like a graph mislabeled as trigger
    f.type = "graph";
    // ensure trigger fields are cleared
    f.idleTemperature = f.loadTemperature = 0.0;
    f.idleFanSpeed = f.loadFanSpeed = 0.0;
}
}

/* ======================== ControlMeta ========================= */

void to_json(json& j, const ControlMeta& c) {
    j = json{
        {"name",     c.name},
        {"pwmPath",  c.pwmPath},
        {"curveRef", c.curveRef},
        {"nickName", c.nickName},
        {"enabled",  c.enabled},
        {"hidden",   c.hidden},
        {"manual",   c.manual},
        {"manualPercent", c.manualPercent}
    };
}
void from_json(const json& j, ControlMeta& c) {
    c = ControlMeta{};
    c.name     = j.value("name", std::string{});
    c.pwmPath  = j.value("pwmPath", std::string{});
    c.curveRef = j.value("curveRef", std::string{});
    if (j.contains("nickName"))      c.nickName = j.at("nickName").get<std::string>();
    else if (j.contains("nick"))     c.nickName = j.at("nick").get<std::string>();
    else if (j.contains("nickname")) c.nickName = j.at("nickname").get<std::string>();
    c.enabled  = j.value("enabled", true);
    c.hidden   = j.value("hidden",  false);
    c.manual   = j.value("manual",  false);
    c.manualPercent = j.value("manualPercent", 0);
}

/* ====================== HwmonDeviceMeta ======================= */

void to_json(json& j, const HwmonDeviceMeta& d) {
    j = json{
        {"hwmonPath", d.hwmonPath},
        {"name",      d.name},
        {"vendor",    d.vendor}
    };
}
void from_json(const json& j, HwmonDeviceMeta& d) {
    d = HwmonDeviceMeta{};
    d.hwmonPath = j.value("hwmonPath", std::string{});
    d.name      = j.value("name", std::string{});
    d.vendor    = j.value("vendor", std::string{});
}

/* ============================ Profile ========================= */

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

/* ===================== File IO (Release) ====================== */

Profile loadProfileFromFile(const std::string& path) {
    json j = util::read_json_file(path);
    return j.get<Profile>();
}

void saveProfileToFile(const Profile& p, const std::string& path) {
    json j = p;
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) throw std::runtime_error("saveProfileToFile: open failed: " + path);
    out << j.dump(4) << "\n";
    if (!out) throw std::runtime_error("saveProfileToFile: write failed: " + path);
}

} // namespace lfc
