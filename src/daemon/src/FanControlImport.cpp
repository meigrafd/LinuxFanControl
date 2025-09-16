/*
 * Linux Fan Control — FanControl.Releases importer
 * Supports:
 *  - Controls[] -> Rule (pwm + selected curve name)
 *  - FanCurves[] -> Table (Points) or Trigger (Idle/Load) or Mix (SelectedFanCurves + SelectedMixFunction)
 *  - Identifier mapping:
 *      /lpc/<chip>/control/<n>       -> hwmon pwm<n+1>
 *      /lpc/<chip>/temperature/<n>   -> hwmon temp<n+1>_input
 *      /amdcpu/.../temperature/<n>   -> label-based CPU temp match
 * (c) 2025 LinuxFanControl contributors
 */
#include "FanControlImport.hpp"
#include "Profile.hpp"
#include "Hwmon.hpp"
#include "Log.hpp"

#include <nlohmann/json.hpp>
#include <fstream>
#include <algorithm>
#include <cctype>
#include <map>

using nlohmann::json;

namespace lfc {

std::string FanControlImport::lower(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

static int parse_index_after(const std::string& s, const std::string& key) {
    auto p = s.rfind("/" + key + "/");
    if (p == std::string::npos) return -1;
    std::string tail = s.substr(p + key.size() + 2);
    auto slash = tail.find('/');
    if (slash != std::string::npos) tail = tail.substr(0, slash);
    try { return std::stoi(tail); } catch (...) { return -1; }
}

std::string FanControlImport::mapPwmFromIdentifier(const std::vector<HwmonPwm>& pwms,
                                                   const std::string& identifier) {
    if (identifier.empty()) return {};
    std::string lid = lower(identifier);

    // Extract desired pwm index (FanControl uses 0-based in Identifier; hwmon is 1-based)
    int idx = parse_index_after(lid, "control");
    int pwmIdx = (idx >= 0) ? (idx + 1) : -1;

    // Optional chip hint (e.g. nct6775)
    std::string chipHint;
    {
        auto p = lid.find("/lpc/");
        if (p != std::string::npos) {
            auto s = lid.substr(p + 5);
            auto e = s.find('/');
            if (e != std::string::npos) chipHint = s.substr(0, e);
        }
    }

    // Prefer chip + pwmIdx: find /pwmX under a path containing chipHint
    if (!chipHint.empty() && pwmIdx > 0) {
        for (const auto& p : pwms) {
            std::string path = lower(p.path_pwm);
            if (path.find(chipHint) != std::string::npos) {
                auto b = path.rfind("/pwm");
                if (b != std::string::npos) {
                    int x = -1;
                    try { x = std::stoi(path.substr(b + 4)); } catch (...) {}
                    if (x == pwmIdx) return p.path_pwm;
                }
            }
        }
    }

    // Fallback: pwmIdx only
    if (pwmIdx > 0) {
        for (const auto& p : pwms) {
            auto b = p.path_pwm.rfind("/pwm");
            if (b != std::string::npos) {
                int x = -1;
                try { x = std::stoi(p.path_pwm.substr(b + 4)); } catch (...) {}
                if (x == pwmIdx) return p.path_pwm;
            }
        }
    }

    // Final fallback: no label available on HwmonPwm; give up
    return {};
}

std::vector<std::string> FanControlImport::mapTempFromIdentifier(const std::vector<HwmonTemp>& temps,
                                                                 const std::string& identifier) {
    std::vector<std::string> out;
    if (identifier.empty()) return out;
    std::string lid = lower(identifier);

    int tidx = parse_index_after(lid, "temperature");
    int tempIdx = (tidx >= 0) ? (tidx + 1) : -1;

    std::string chipHint;
    {
        auto p = lid.find("/lpc/");
        if (p != std::string::npos) {
            auto s = lid.substr(p + 5);
            auto e = s.find('/');
            if (e != std::string::npos) chipHint = s.substr(0, e);
        }
    }

    // chip + index
    if (!chipHint.empty() && tempIdx > 0) {
        for (const auto& t : temps) {
            std::string path = lower(t.path_input);
            if (path.find(chipHint) != std::string::npos) {
                auto b = path.rfind("/temp");
                if (b != std::string::npos) {
                    int x = -1;
                    try {
                        auto rest = path.substr(b + 5);
                        auto us = rest.find('_');
                        if (us != std::string::npos) rest = rest.substr(0, us);
                        x = std::stoi(rest);
                    } catch (...) {}
                    if (x == tempIdx) {
                        out.push_back(t.path_input);
                        return out;
                    }
                }
            }
        }
    }

    // index only
    if (tempIdx > 0) {
        for (const auto& t : temps) {
            auto b = t.path_input.rfind("/temp");
            if (b != std::string::npos) {
                int x = -1;
                try {
                    auto rest = t.path_input.substr(b + 5);
                    auto us = rest.find('_');
                    if (us != std::string::npos) rest = rest.substr(0, us);
                    x = std::stoi(rest);
                } catch (...) {}
                if (x == tempIdx) {
                    out.push_back(t.path_input);
                    return out;
                }
            }
        }
    }

    // AMD CPU identifiers: try label-based CPU temp
    if (lid.find("/amdcpu/") != std::string::npos) {
        for (const auto& t : temps) {
            std::string lbl = lower(t.label.empty() ? t.path_input : t.label);
            if (lbl.find("tctl") != std::string::npos || lbl.find("cpu") != std::string::npos ||
                lbl.find("pkg") != std::string::npos || lbl.find("tz") != std::string::npos) {
                out.push_back(t.path_input);
                return out;
            }
        }
    }

    return out;
}

void FanControlImport::parsePointsArray(const json& arr, std::vector<std::pair<double,int>>& dst) {
    dst.clear();
    if (!arr.is_array()) return;
    for (const auto& s : arr) {
        if (!s.is_string()) continue;
        std::string v = s.get<std::string>();
        auto comma = v.find(',');
        if (comma == std::string::npos) continue;
        try {
            double t = std::stod(v.substr(0, comma));
            double p = std::stod(v.substr(comma + 1));
            int ip = static_cast<int>(p + 0.5);
            dst.emplace_back(t, std::max(0, std::min(100, ip)));
        } catch (...) {}
    }
    std::sort(dst.begin(), dst.end(), [](const auto& a, const auto& b){ return a.first < b.first; });
}

void FanControlImport::buildTriggerPoints(double idleTemp, int idlePct, double loadTemp, int loadPct,
                                          std::vector<std::pair<double,int>>& dst) {
    dst.clear();
    if (loadTemp < idleTemp) {
        std::swap(loadTemp, idleTemp);
        std::swap(loadPct, idlePct);
    }
    dst.emplace_back(idleTemp, std::max(0, std::min(100, idlePct)));
    dst.emplace_back(loadTemp, std::max(0, std::min(100, loadPct)));
}

bool FanControlImport::buildSourceFromCurveJson(const json& cj,
                                                const std::vector<HwmonTemp>& temps,
                                                std::vector<std::string>& outTempPaths,
                                                std::vector<std::pair<double,int>>& outPoints,
                                                int& outMinPercent, int& outMaxPercent, double& outHystC,
                                                int& outSpinupPercent, int& outSpinupMs,
                                                bool& outStopBelowMin,
                                                std::string& err) {
    err.clear();
    outTempPaths.clear();
    outPoints.clear();
    outMinPercent = 0;
    outMaxPercent = 100;
    outHystC = 0.0;
    outSpinupPercent = 0;
    outSpinupMs = 0;
    outStopBelowMin = false;

    if (cj.contains("Points") && cj["Points"].is_array()) {
        parsePointsArray(cj["Points"], outPoints);
    } else if (cj.contains("LoadFanSpeed") && cj.contains("LoadTemperature") &&
               cj.contains("IdleFanSpeed") && cj.contains("IdleTemperature")) {
        double iT = cj["IdleTemperature"].get<double>();
        int    iP = cj["IdleFanSpeed"].get<int>();
        double lT = cj["LoadTemperature"].get<double>();
        int    lP = cj["LoadFanSpeed"].get<int>();
        buildTriggerPoints(iT, iP, lT, lP, outPoints);
    } // mix/reference handled by caller

    if (cj.contains("MaximumCommand") && cj["MaximumCommand"].is_number()) {
        outMaxPercent = std::max(0, std::min(100, static_cast<int>(cj["MaximumCommand"].get<double>() + 0.5)));
    }
    if (cj.contains("MinimumPercent") && cj["MinimumPercent"].is_number()) {
        outMinPercent = std::max(0, std::min(100, cj["MinimumPercent"].get<int>()));
    }
    if (cj.contains("HysteresisConfig") && cj["HysteresisConfig"].is_object()) {
        auto& hc = cj["HysteresisConfig"];
        if (hc.contains("HysteresisValueDown") && hc["HysteresisValueDown"].is_number()) {
            outHystC = hc["HysteresisValueDown"].get<double>();
        } else if (hc.contains("HysteresisValueUp") && hc["HysteresisValueUp"].is_number()) {
            outHystC = hc["HysteresisValueUp"].get<double>();
        }
    }
    if (cj.contains("SelectedTempSource") && cj["SelectedTempSource"].is_object()) {
        auto& ts = cj["SelectedTempSource"];
        if (ts.contains("Identifier") && ts["Identifier"].is_string()) {
            auto id = ts["Identifier"].get<std::string>();
            outTempPaths = mapTempFromIdentifier(temps, id);
        }
    }
    return true;
}

bool FanControlImport::LoadAndMap(const std::string& path,
                                  const std::vector<HwmonTemp>& temps,
                                  const std::vector<HwmonPwm>& pwms,
                                  Profile& out,
                                  std::string& err) {
    err.clear();
    out = Profile{};

    std::ifstream f(path);
    if (!f) { err = "open failed"; return false; }
    json root;
    try { f >> root; } catch (const std::exception& e) { err = std::string("json parse: ") + e.what(); return false; }

    if (!root.contains("__VERSION__") || !root.contains("Main") || !root["Main"].is_object()) {
        err = "unsupported format";
        return false;
    }

    const json& main = root["Main"];

    std::map<std::string, json> curveByName;
    if (main.contains("FanCurves") && main["FanCurves"].is_array()) {
        for (const auto& cj : main["FanCurves"]) {
            if (cj.contains("Name") && cj["Name"].is_string()) {
                curveByName[cj["Name"].get<std::string>()] = cj;
            }
        }
    }

    if (!main.contains("Controls") || !main["Controls"].is_array()) {
        err = "no Controls array";
        return false;
    }

    for (const auto& ctrl : main["Controls"]) {
        if (ctrl.contains("Enable") && ctrl["Enable"].is_boolean() && !ctrl["Enable"].get<bool>()) {
            continue;
        }

        Rule rule;

        // PWM path from identifier
        std::string id = ctrl.value("Identifier", std::string());
        rule.pwmPath = mapPwmFromIdentifier(pwms, id);
        if (rule.pwmPath.empty()) continue;

        // Curve name
        std::string curveName;
        if (ctrl.contains("SelectedFanCurve") && ctrl["SelectedFanCurve"].is_object()) {
            auto& sc = ctrl["SelectedFanCurve"];
            if (sc.contains("Name") && sc["Name"].is_string()) {
                curveName = sc["Name"].get<std::string>();
            }
        }
        if (curveName.empty()) continue;

        auto it = curveByName.find(curveName);
        if (it == curveByName.end()) continue;
        const json& cj = it->second;

        // Mix?
        if (cj.contains("SelectedFanCurves") && cj["SelectedFanCurves"].is_array()) {
            int mix = cj.value("SelectedMixFunction", 2); // 2 = Max
            rule.mixFn = (mix == 1) ? MixFunction::Avg : MixFunction::Max;

            for (const auto& ref : cj["SelectedFanCurves"]) {
                if (!ref.is_object() || !ref.contains("Name") || !ref["Name"].is_string()) continue;
                std::string refName = ref["Name"].get<std::string>();
                auto it2 = curveByName.find(refName);
                if (it2 == curveByName.end()) continue;

                std::vector<std::string> tpaths;
                std::vector<std::pair<double,int>> pts;
                int minP=0, maxP=100, spinP=0, spinMs=0; double hyst=0.0; bool stopMin=false;
                std::string e2;
                (void)buildSourceFromCurveJson(it2->second, temps, tpaths, pts, minP, maxP, hyst, spinP, spinMs, stopMin, e2);

                SourceCurve sc;
                sc.settings.minPercent = minP;
                sc.settings.maxPercent = maxP;
                sc.settings.hysteresisC = hyst;
                sc.settings.spinupPercent = spinP;
                sc.settings.spinupMs = spinMs;
                sc.settings.stopBelowMin = stopMin;
                for (auto& pnt : pts) sc.points.push_back({pnt.first, pnt.second});
                sc.tempPaths = std::move(tpaths);
                if (!sc.points.empty()) rule.sources.push_back(std::move(sc));
            }
        } else {
            // Single table/trigger
            std::vector<std::string> tpaths;
            std::vector<std::pair<double,int>> pts;
            int minP=0, maxP=100, spinP=0, spinMs=0; double hyst=0.0; bool stopMin=false;
            std::string e2;
            (void)buildSourceFromCurveJson(cj, temps, tpaths, pts, minP, maxP, hyst, spinP, spinMs, stopMin, e2);

            SourceCurve sc;
            sc.settings.minPercent = minP;
            sc.settings.maxPercent = maxP;
            sc.settings.hysteresisC = hyst;
            sc.settings.spinupPercent = spinP;
            sc.settings.spinupMs = spinMs;
            sc.settings.stopBelowMin = stopMin;
            for (auto& pnt : pts) sc.points.push_back({pnt.first, pnt.second});
            sc.tempPaths = std::move(tpaths);
            if (!sc.points.empty()) rule.sources.push_back(std::move(sc));
            rule.mixFn = MixFunction::Max;
        }

        // Control-level MinimumPercent
        if (ctrl.contains("MinimumPercent") && ctrl["MinimumPercent"].is_number_integer()) {
            int minCtrl = ctrl["MinimumPercent"].get<int>();
            for (auto& sc : rule.sources) sc.settings.minPercent = std::max(sc.settings.minPercent, minCtrl);
        }

        if (!rule.sources.empty()) out.rules.push_back(std::move(rule));
    }

    if (out.rules.empty()) {
        err = "no usable controls after mapping";
        return false;
    }
    return true;
}

} // namespace lfc
