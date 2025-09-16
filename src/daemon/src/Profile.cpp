/*
 * Linux Fan Control — Profile model & loader (implementation)
 * (c) 2025 LinuxFanControl contributors
 */
#include "Profile.hpp"

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <nlohmann/json.hpp>

namespace lfc {

using nlohmann::json;

static std::string readFile(const std::string& path) {
    std::ifstream f(path);
    std::ostringstream oss;
    oss << f.rdbuf();
    return oss.str();
}

static void from_json(const json& j, Profile::StringRef& r) {
    if (j.is_object()) {
        r.Name = j.value("Name", "");
    } else if (j.is_string()) {
        r.Name = j.get<std::string>();
    }
}

static void from_json(const json& j, Profile::TempSource& t) {
    if (j.is_object()) {
        t.Identifier = j.value("Identifier", "");
    } else if (j.is_string()) {
        t.Identifier = j.get<std::string>();
    }
}

static void from_json(const json& j, Profile::Curve& c) {
    c.Name               = j.value("Name", "");
    c.CommandMode        = j.value("CommandMode", 0);

    if (j.contains("SelectedTempSource")) {
        from_json(j.at("SelectedTempSource"), c.SelectedTempSource);
    }
    c.MaximumTemperature = j.value("MaximumTemperature", 100);
    c.MinimumTemperature = j.value("MinimumTemperature", 0);
    c.MaximumCommand     = j.value("MaximumCommand", 100);
    if (j.contains("Points") && j.at("Points").is_array()) {
        c.Points.clear();
        for (const auto& p : j.at("Points")) {
            if (p.is_string()) c.Points.push_back(p.get<std::string>());
        }
    }

    c.SelectedMixFunction = j.value("SelectedMixFunction", 0);
    if (j.contains("SelectedFanCurves") && j.at("SelectedFanCurves").is_array()) {
        c.SelectedFanCurves.clear();
        for (const auto& r : j.at("SelectedFanCurves")) {
            Profile::StringRef sr{};
            from_json(r, sr);
            c.SelectedFanCurves.push_back(std::move(sr));
        }
    }

    if (j.contains("TriggerTempSource")) {
        from_json(j.at("TriggerTempSource"), c.TriggerTempSource);
    }
    c.LoadFanSpeed     = j.value("LoadFanSpeed", 100);
    c.LoadTemperature  = j.value("LoadTemperature", 80);
    c.IdleFanSpeed     = j.value("IdleFanSpeed", 20);
    c.IdleTemperature  = j.value("IdleTemperature", 40);
}

static void from_json(const json& j, Profile::Control& c) {
    c.Enable          = j.value("Enable", true);
    c.MinimumPercent  = j.value("MinimumPercent", 0);
    c.Identifier      = j.value("Identifier", "");
    if (j.contains("SelectedFanCurve")) {
        from_json(j.at("SelectedFanCurve"), c.SelectedFanCurve);
    }
}

bool Profile::loadFromFile(const std::string& path, std::string* err) {
    try {
        auto s = readFile(path);
        return loadFromJson(s, err);
    } catch (const std::exception& ex) {
        if (err) *err = std::string("read error: ") + ex.what();
        return false;
    }
}

bool Profile::loadFromJson(const std::string& jsonText, std::string* err) {
    try {
        auto j = json::parse(jsonText);

        Controls.clear();
        FanCurves.clear();

        if (j.contains("Controls") && j.at("Controls").is_array()) {
            for (const auto& it : j.at("Controls")) {
                Control c{};
                from_json(it, c);
                Controls.push_back(std::move(c));
            }
        }

        if (j.contains("FanCurves") && j.at("FanCurves").is_array()) {
            for (const auto& it : j.at("FanCurves")) {
                Curve c{};
                from_json(it, c);
                FanCurves.push_back(std::move(c));
            }
        }

        return true;
    } catch (const std::exception& ex) {
        if (err) *err = std::string("json parse error: ") + ex.what();
        return false;
    }
}

} // namespace lfc
