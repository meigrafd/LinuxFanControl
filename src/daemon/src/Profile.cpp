/*
 * Linux Fan Control — Profile model (JSON I/O)
 * (c) 2025 LinuxFanControl contributors
 */
#include "Profile.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <sstream>
#include <algorithm>

namespace lfc {

using nlohmann::json;

// ---------- helpers ----------

static int clampi(int v, int lo, int hi) { return std::max(lo, std::min(hi, v)); }
static double clampd(double v, double lo, double hi) { return std::max(lo, std::min(hi, v)); }

static MixFunction parseMix(const json& j) {
    if (j.is_string()) {
        std::string s = j.get<std::string>();
        std::transform(s.begin(), s.end(), s.begin(), ::tolower);
        if (s == "avg" || s == "average") return MixFunction::Avg;
        return MixFunction::Max;
    }
    if (j.is_number_integer()) {
        int v = j.get<int>();
        return (v == 1) ? MixFunction::Avg : MixFunction::Max;
    }
    return MixFunction::Max;
}

static SourceKind parseKind(const json& j) {
    if (j.is_string()) {
        std::string s = j.get<std::string>();
        std::transform(s.begin(), s.end(), s.begin(), ::tolower);
        if (s == "trigger") return SourceKind::Trigger;
        return SourceKind::Table;
    }
    if (j.is_number_integer()) {
        int v = j.get<int>();
        return (v == 1) ? SourceKind::Trigger : SourceKind::Table;
    }
    return SourceKind::Table;
}

// ---------- JSON -> structs ----------

static void from_json(const json& j, CurvePoint& p) {
    if (!j.is_object()) return;
    if (j.contains("tempC"))   p.tempC   = j["tempC"].get<double>();
    if (j.contains("percent")) p.percent = clampi(j["percent"].get<int>(), 0, 100);
}

static void from_json(const json& j, SourceSettings& s) {
    if (!j.is_object()) return;
    if (j.contains("minPercent"))   s.minPercent   = clampi(j["minPercent"].get<int>(), 0, 100);
    if (j.contains("maxPercent"))   s.maxPercent   = clampi(j["maxPercent"].get<int>(), 0, 100);
    if (j.contains("stopBelowMin")) s.stopBelowMin = j["stopBelowMin"].get<bool>();
    if (j.contains("hysteresisC"))  s.hysteresisC  = clampd(j["hysteresisC"].get<double>(), 0.0, 100.0);
    if (j.contains("spinupPercent")) s.spinupPercent = clampi(j["spinupPercent"].get<int>(), 0, 100);
    if (j.contains("spinupMs"))      s.spinupMs      = std::max(0, j["spinupMs"].get<int>());
}

static void from_json(const json& j, SourceCurve& sc) {
    if (!j.is_object()) return;

    sc.label.clear();
    if (j.contains("label") && j["label"].is_string())
        sc.label = j["label"].get<std::string>();

    if (j.contains("kind"))
        sc.kind = parseKind(j["kind"]);
    else
        sc.kind = SourceKind::Table; // backward compatibility

    sc.tempPaths.clear();
    if (j.contains("paths") && j["paths"].is_array()) {
        for (const auto& p : j["paths"]) sc.tempPaths.push_back(p.get<std::string>());
    } else if (j.contains("tempPaths") && j["tempPaths"].is_array()) {
        for (const auto& p : j["tempPaths"]) sc.tempPaths.push_back(p.get<std::string>());
    }

    sc.points.clear();
    if (j.contains("points") && j["points"].is_array()) {
        for (const auto& jp : j["points"]) {
            CurvePoint cp{}; from_json(jp, cp);
            sc.points.push_back(std::move(cp));
        }
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

    if (j.contains("mix"))        r.mixFn = parseMix(j["mix"]);
    else if (j.contains("mixFn")) r.mixFn = parseMix(j["mixFn"]);
}

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

static const char* kind_to_cstr(SourceKind k) {
    return (k == SourceKind::Trigger) ? "trigger" : "table";
}

static void to_json(json& j, const SourceCurve& sc) {
    j = json{};
    if (!sc.label.empty()) j["label"] = sc.label;
    j["kind"]   = kind_to_cstr(sc.kind);
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
