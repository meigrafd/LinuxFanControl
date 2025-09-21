/*
 * Linux Fan Control — Profile (implementation)
 * - JSON serialization/deserialization and helpers
 * (c) 2025 LinuxFanControl contributors
 */
#include "include/Profile.hpp"
#include "include/Hwmon.hpp"

#include <nlohmann/json.hpp>
#include <fstream>
#include <stdexcept>
#include <vector>

namespace lfc {

using nlohmann::json;

// --- JSON for low-level hwmon structs (needed for HwmonDeviceMeta.pwms) -----

void to_json(json& j, const HwmonTemp& t) {
    j = json{
        {"chip_path",  t.chipPath},
        {"path_input", t.path_input},
        {"label",      t.label}
    };
}

void from_json(const json& j, HwmonTemp& t) {
    t.chipPath  = j.value("chip_path",  std::string{});
    t.path_input = j.value("path_input", std::string{});
    t.label      = j.value("label",      std::string{});
}

void to_json(json& j, const HwmonFan& f) {
    j = json{
        {"chip_path",  f.chipPath},
        {"path_input", f.path_input},
        {"label",      f.label}
    };
}

void from_json(const json& j, HwmonFan& f) {
    f.chipPath  = j.value("chip_path",  std::string{});
    f.path_input = j.value("path_input", std::string{});
    f.label      = j.value("label",      std::string{});
}

void to_json(json& j, const HwmonPwm& p) {
    j = json{
        {"chip_path",   p.chipPath},
        {"path_pwm",    p.path_pwm},
        {"path_enable", p.path_enable},
        {"pwm_max",     p.pwm_max},
        {"label",       p.label}
    };
}

void from_json(const json& j, HwmonPwm& p) {
    p.chipPath   = j.value("chip_path",   std::string{});
    p.path_pwm    = j.value("path_pwm",    std::string{});
    p.path_enable = j.value("path_enable", std::string{});
    p.pwm_max     = j.value("pwm_max",     255);
    p.label       = j.value("label",       std::string{});
}

// --- JSON for profile model ---------------------------------------------------

void to_json(json& j, const CurvePoint& p) {
    j = json{{"tempC", p.tempC}, {"percent", p.percent}};
}

void from_json(const json& j, CurvePoint& p) {
    p.tempC = j.value("tempC", 0.0);
    p.percent = j.value("percent", 0);
}

void to_json(json& j, const FanCurveMeta& f) {
    j = json{
        {"name", f.name},
        {"type", f.type},
        {"mix", (f.mix == MixFunction::Min ? "Min" :
                 f.mix == MixFunction::Max ? "Max" : "Avg")},
        {"tempSensors", f.tempSensors},
        {"points", f.points},
        {"onC", f.onC},
        {"offC", f.offC}
    };
}

void from_json(const json& j, FanCurveMeta& f) {
    f.name = j.value("name", "");
    f.type = j.value("type", "graph");
    std::string mixStr = j.value("mix", "Avg");
    if (mixStr == "Min") f.mix = MixFunction::Min;
    else if (mixStr == "Max") f.mix = MixFunction::Max;
    else f.mix = MixFunction::Avg;
    f.tempSensors = j.value("tempSensors", std::vector<std::string>{});
    f.points = j.value("points", std::vector<CurvePoint>{});
    f.onC = j.value("onC", 0.0);
    f.offC = j.value("offC", 0.0);
}

void to_json(json& j, const ControlMeta& c) {
    j = json{{"name", c.name}, {"pwmPath", c.pwmPath}, {"curveRef", c.curveRef}};
}

void from_json(const json& j, ControlMeta& c) {
    c.name = j.value("name", "");
    c.pwmPath = j.value("pwmPath", "");
    c.curveRef = j.value("curveRef", "");
}

void to_json(json& j, const HwmonDeviceMeta& d) {
    j = json{
        {"hwmonPath", d.hwmonPath},
        {"name",      d.name},
        {"vendor",    d.vendor},
        {"pwms",      d.pwms}
    };
}

void from_json(const json& j, HwmonDeviceMeta& d) {
    d.hwmonPath = j.value("hwmonPath", "");
    d.name      = j.value("name", "");
    d.vendor    = j.value("vendor", "");
    d.pwms      = j.value("pwms", std::vector<HwmonPwm>{});
}

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
    p.schema       = j.value("schema", "");
    p.name         = j.value("name", "");
    p.description  = j.value("description", "");
    p.lfcdVersion  = j.value("lfcdVersion", "");
    p.fanCurves    = j.value("fanCurves", std::vector<FanCurveMeta>{});
    p.controls     = j.value("controls", std::vector<ControlMeta>{});
    p.hwmons       = j.value("hwmons", std::vector<HwmonDeviceMeta>{});
}

// --- helpers ------------------------------------------------------------------

Profile loadProfileFromFile(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) throw std::runtime_error("Cannot open profile: " + path);
    json j;
    f >> j;
    return j.get<Profile>();
}

void saveProfileToFile(const Profile& p, const std::string& path) {
    json j = p;
    std::ofstream f(path);
    if (!f.is_open()) throw std::runtime_error("Cannot save profile: " + path);
    f << j.dump(4);
}

} // namespace lfc
