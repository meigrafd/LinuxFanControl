/*
 * Linux Fan Control — Profile JSON I/O
 * - Strict but forgiving (type checks, defaults)
 * - Temperatures in °C; percent clamped to 0..100
 * (c) 2025 LinuxFanControl contributors
 */
#include "Profile.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>

using nlohmann::json;

namespace lfc {

// ---------- helpers ----------

static int clampi(int v, int lo, int hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

static double clampd(double v, double lo, double hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

static bool j_is_strict_array_of_strings(const json& j) {
    if (!j.is_array()) return false;
    for (const auto& x : j) if (!x.is_string()) return false;
    return true;
}

static MixFunction parseMix(const json& j) {
    if (j.is_string()) {
        auto s = j.get<std::string>();
        for (auto& c : s) c = static_cast<char>(::tolower(static_cast<unsigned char>(c)));
        if (s == "avg" || s == "average") return MixFunction::Avg;
        return MixFunction::Max;
    }
    if (j.is_number_integer()) {
        int v = j.get<int>();
        return v == 1 ? MixFunction::Avg : MixFunction::Max;
    }
    return MixFunction::Max;
}

// ---------- JSON -> structs ----------

static void from_json(const json& j, CurvePoint& p) {
    if (j.is_object()) {
        if (j.contains("t")) p.tempC = j.value("t", 0.0);
        else if (j.contains("tempC")) p.tempC = j.value("tempC", 0.0);
        if (j.contains("y")) p.percent = clampi(j.value("y", 0), 0, 100);
        else if (j.contains("percent")) p.percent = clampi(j.value("percent", 0), 0, 100);
    } else if (j.is_array() && j.size() >= 2) {
        p.tempC  = j[0].is_number() ? j[0].get<double>() : 0.0;
        p.percent = j[1].is_number_integer() ? clampi(j[1].get<int>(), 0, 100) : 0;
    }
}

static void from_json(const json& j, SourceSettings& s) {
    if (!j.is_object()) return;
    if (j.contains("minPercent"))    s.minPercent    = clampi(j.value("minPercent", s.minPercent), 0, 100);
    if (j.contains("maxPercent"))    s.maxPercent    = clampi(j.value("maxPercent", s.maxPercent), 0, 100);
    if (j.contains("stopBelowMin"))  s.stopBelowMin  = j.value("stopBelowMin", s.stopBelowMin);
    if (j.contains("hysteresisC"))   s.hysteresisC   = clampd(j.value("hysteresisC", s.hysteresisC), 0.0, 100.0);
    if (j.contains("spinupPercent")) s.spinupPercent = clampi(j.value("spinupPercent", s.spinupPercent), 0, 100);
    if (j.contains("spinupMs"))      s.spinupMs      = std::max(0, j.value("spinupMs", s.spinupMs));
}

static void from_json(const json& j, SourceCurve& sc) {
    if (!j.is_object()) return;

    if (j.contains("paths") && j_is_strict_array_of_strings(j["paths"])) {
        sc.tempPaths.clear();
        for (const auto& s : j["paths"]) sc.tempPaths.push_back(s.get<std::string>());
    } else if (j.contains("path") && j["path"].is_string()) {
        sc.tempPaths = { j["path"].get<std::string>() };
    }

    sc.points.clear();
    if (j.contains("points") && j["points"].is_array()) {
        for (const auto& jp : j["points"]) {
            CurvePoint p{};
            from_json(jp, p);
            sc.points.push_back(p);
        }
        std::sort(sc.points.begin(), sc.points.end(),
                  [](const CurvePoint& a, const CurvePoint& b){ return a.tempC < b.tempC; });
    }

    if (j.contains("settings")) from_json(j["settings"], sc.settings);
}

static void from_json(const json& j, Rule& r) {
    if (!j.is_object()) return;

    if (j.contains("pwm") && j["pwm"].is_string()) {
        r.pwmPath = j["pwm"].get<std::string>();
    } else if (j.contains("pwmPath") && j["pwmPath"].is_string()) {
        r.pwmPath = j["pwmPath"].get<std::string>();
    }

    r.sources.clear();
    if (j.contains("sources") && j["sources"].is_array()) {
        for (const auto& js : j["sources"]) {
            SourceCurve sc{};
            from_json(js, sc);
            r.sources.push_back(std::move(sc));
        }
    } else if (j.contains("source") && j["source"].is_object()) {
        SourceCurve sc{};
        from_json(j["source"], sc);
        r.sources.push_back(std::move(sc));
    }

    if (j.contains("mix"))     r.mixFn = parseMix(j["mix"]);
    else if (j.contains("mixFn")) r.mixFn = parseMix(j["mixFn"]);
}

// Profile (adds optional human-readable name)
static void from_json(const json& j, Profile& prof) {
    prof.name.clear();
    prof.rules.clear();
    if (!j.is_object()) return;

    if (j.contains("name") && j["name"].is_string())
        prof.name = j["name"].get<std::string>();

    if (j.contains("rules") && j["rules"].is_array()) {
        for (const auto& jr : j["rules"]) {
            Rule r{};
            from_json(jr, r);
            if (!r.pwmPath.empty()) prof.rules.push_back(std::move(r));
        }
    }
}

// ---------- structs -> JSON ----------

static void to_json(json& j, const CurvePoint& p) {
    j = json{{"tempC", p.tempC}, {"percent", clampi(p.percent, 0, 100)}};
}

static void to_json(json& j, const SourceSettings& s) {
    j = json{
        {"minPercent",    clampi(s.minPercent, 0, 100)},
        {"maxPercent",    clampi(s.maxPercent, 0, 100)},
        {"stopBelowMin",  s.stopBelowMin},
        {"hysteresisC",   clampd(s.hysteresisC, 0.0, 100.0)},
        {"spinupPercent", clampi(s.spinupPercent, 0, 100)},
        {"spinupMs",      std::max(0, s.spinupMs)}
    };
}

static void to_json(json& j, const SourceCurve& sc) {
    j = json{};
    j["paths"]  = sc.tempPaths;
    j["points"] = json::array();
    for (const auto& p : sc.points) {
        json jp; to_json(jp, p);
        j["points"].push_back(std::move(jp));
    }
    json js; to_json(js, sc.settings);
    j["settings"] = std::move(js);
}

static void to_json(json& j, const Rule& r) {
    j = json{};
    j["pwm"] = r.pwmPath;
    j["sources"] = json::array();
    for (const auto& s : r.sources) {
        json js; to_json(js, s);
        j["sources"].push_back(std::move(js));
    }
    j["mix"] = (r.mixFn == MixFunction::Avg ? "avg" : "max");
}

static void to_json(json& j, const Profile& prof) {
    j = json{};
    if (!prof.name.empty()) j["name"] = prof.name;
    j["rules"] = json::array();
    for (const auto& r : prof.rules) {
        json jr; to_json(jr, r);
        j["rules"].push_back(std::move(jr));
    }
}

// ---------- file I/O ----------

bool Profile::loadFromFile(const std::string& path, std::string* err) {
    try {
        std::ifstream f(path);
        if (!f) {
            if (err) *err = "open failed: " + path;
            return false;
        }
        std::ostringstream ss; ss << f.rdbuf();
        auto j = json::parse(ss.str(), nullptr, true);
        from_json(j, *this);
        return true;
    } catch (const std::exception& ex) {
        if (err) *err = ex.what();
        return false;
    }
}

bool Profile::saveToFile(const std::string& path, std::string* err) const {
    try {
        json j; to_json(j, *this);
        std::ofstream f(path);
        if (!f) {
            if (err) *err = "open failed: " + path;
            return false;
        }
        f << j.dump(2) << "\n";
        return true;
    } catch (const std::exception& ex) {
        if (err) *err = ex.what();
        return false;
    }
}

} // namespace lfc
