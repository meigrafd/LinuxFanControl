/*
 * Linux Fan Control — FanControl.Releases importer (implementation)
 * Maps upstream JSON schema to our Profile (incl. Mix/Trigger)
 * (c) 2025 LinuxFanControl contributors
 */
#include "FanControlImport.hpp"

#include "Hwmon.hpp"
#include "Profile.hpp"

#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <regex>
#include <cctype>
#include <algorithm>
#include <unordered_map>

namespace lfc {

using nlohmann::json;
namespace fs = std::filesystem;

// utils

static std::string basename_noext(const std::string& p) {
    fs::path x{p};
    return x.stem().string();
}

std::string FanControlImport::lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
    return s;
}

std::string FanControlImport::mapPwmFromIdentifier(const std::vector<HwmonPwm>& pwms,
                                                   const std::string& identifier) {
    std::regex re(R"(\/control\/([0-9]+))");
    std::smatch m;
    int want = -1;
    if (std::regex_search(identifier, m, re) && m.size() >= 2) {
        want = std::atoi(m[1].str().c_str());
    }
    if (want >= 0) {
        for (const auto& p : pwms) {
            auto fn = fs::path(p.path_pwm).filename().string();
            if (lower(fn) == ("pwm" + std::to_string(want))) return p.path_pwm;
        }
    }
    return pwms.empty() ? std::string{} : pwms.front().path_pwm;
}

std::vector<std::string> FanControlImport::mapTempFromIdentifier(const std::vector<HwmonTemp>& temps,
                                                                 const std::string& identifier) {
    std::vector<std::string> out;

    // numeric index from .../temperature/N
    std::regex re(R"(\/temperature\/([0-9]+))");
    std::smatch m;
    if (std::regex_search(identifier, m, re) && m.size() >= 2) {
        int want = std::atoi(m[1].str().c_str());
        std::string needle = "temp" + std::to_string(want) + "_input";
        for (const auto& t : temps) {
            auto fn = fs::path(t.path_input).filename().string();
            if (lower(fn) == lower(needle)) out.push_back(t.path_input);
        }
        if (!out.empty()) return out;
    }

    // keyword match by label/path
    const std::string idlow = lower(identifier);
    auto match_kw = [&](const std::string& kw){
        for (const auto& t : temps) {
            if (!t.label.empty()) {
                auto ll = lower(t.label);
                if (ll.find(kw) != std::string::npos) out.push_back(t.path_input);
            } else {
                auto pathl = lower(t.path_input);
                if (pathl.find(kw) != std::string::npos) out.push_back(t.path_input);
            }
        }
    };
    if (idlow.find("hotspot") != std::string::npos) match_kw("hotspot");
    if (idlow.find("gpu")     != std::string::npos) match_kw("gpu");
    if (idlow.find("cpu")     != std::string::npos) match_kw("cpu");
    if (idlow.find("water")   != std::string::npos) match_kw("water");
    if (idlow.find("ambient") != std::string::npos) match_kw("ambient");

    if (!out.empty()) return out;
    if (!temps.empty()) out.push_back(temps.front().path_input);
    return out;
}

void FanControlImport::parsePointsArray(const json& arr, std::vector<std::pair<double,int>>& dst) {
    dst.clear();
    if (!arr.is_array()) return;
    for (const auto& p : arr) {
        if (!p.is_object()) continue;
        if (p.contains("Temperature") && p.contains("Command")
            && p["Temperature"].is_number() && p["Command"].is_number()) {
            double t = p["Temperature"].get<double>();
            int    c = p["Command"].get<int>();
            dst.emplace_back(t, std::clamp(c, 0, 100));
        }
    }
    std::sort(dst.begin(), dst.end(), [](auto& a, auto& b){ return a.first < b.first; });
}

void FanControlImport::buildTriggerPoints(double idleTemp, int idlePct, double loadTemp, int loadPct,
                                          std::vector<std::pair<double,int>>& dst) {
    dst.clear();
    dst.emplace_back(idleTemp, std::clamp(idlePct, 0, 100));
    if (loadTemp < idleTemp) loadTemp = idleTemp;
    dst.emplace_back(loadTemp, std::clamp(loadPct, 0, 100));
}

bool FanControlImport::buildSourceFromCurveJson(const json& cj,
                                                const std::vector<HwmonTemp>& temps,
                                                std::vector<std::string>& outTempPaths,
                                                std::vector<std::pair<double,int>>& outPoints,
                                                int& outMinPercent, int& outMaxPercent, double& outHystC,
                                                int& outSpinupPercent, int& outSpinupMs,
                                                bool& outStopBelowMin,
                                                std::string& err)
{
    err.clear();
    outTempPaths.clear();
    outPoints.clear();
    outMinPercent = 0;
    outMaxPercent = 100;
    outHystC = 0.0;
    outSpinupPercent = 0;
    outSpinupMs = 0;
    outStopBelowMin = false;

    if (!cj.is_object()) { err = "bad curve json"; return false; }

    // temperature source
    if (cj.contains("SelectedTempSource") && cj["SelectedTempSource"].is_object()) {
        const auto& ts = cj["SelectedTempSource"];
        if (ts.contains("Identifier") && ts["Identifier"].is_string()) {
            outTempPaths = mapTempFromIdentifier(temps, ts["Identifier"].get<std::string>());
        }
    }

    const bool isTrigger = (cj.contains("IdleFanSpeed") && cj.contains("LoadFanSpeed") &&
                            cj.contains("IdleTemperature") && cj.contains("LoadTemperature"));

    if (isTrigger) {
        double idleT = cj["IdleTemperature"].get<double>();
        double loadT = cj["LoadTemperature"].get<double>();
        int idleC = cj["IdleFanSpeed"].get<int>();
        int loadC = cj["LoadFanSpeed"].get<int>();
        buildTriggerPoints(idleT, idleC, loadT, loadC, outPoints);
    } else {
        if (cj.contains("Points")) parsePointsArray(cj["Points"], outPoints);
        if (outPoints.empty()) { err = "empty points"; return false; }

        if (cj.contains("HysteresisConfig") && cj["HysteresisConfig"].is_array() && !cj["HysteresisConfig"].empty()) {
            auto a = cj["HysteresisConfig"][0];
            if (a.is_number()) outHystC = std::max(0.0, a.get<double>());
        }
        if (cj.contains("MaximumCommand") && cj["MaximumCommand"].is_number_integer())
            outMaxPercent = std::clamp(cj["MaximumCommand"].get<int>(), 0, 100);
        if (cj.contains("StopFanBelowMinBool") && cj["StopFanBelowMinBool"].is_boolean())
            outStopBelowMin = cj["StopFanBelowMinBool"].get<bool>();
        if (cj.contains("SpinUpTime") && cj["SpinUpTime"].is_number_integer())
            outSpinupMs = std::max(0, cj["SpinUpTime"].get<int>());
        if (cj.contains("SpinUpCommand") && cj["SpinUpCommand"].is_number_integer())
            outSpinupPercent = std::clamp(cj["SpinUpCommand"].get<int>(), 0, 100);
    }

    return !outPoints.empty();
}

static MixFunction mix_from_int(int v) {
    return (v == 1) ? MixFunction::Avg : MixFunction::Max;
}

bool FanControlImport::LoadAndMap(const std::string& path,
                                  const std::vector<HwmonTemp>& temps,
                                  const std::vector<HwmonPwm>& pwms,
                                  Profile& out,
                                  std::string& err)
{
    err.clear();
    out = Profile{};

    // load JSON
    json root;
    try {
        std::ifstream f(path);
        if (!f) { err = "open failed"; return false; }
        std::ostringstream ss; ss << f.rdbuf();
        root = json::parse(ss.str(), nullptr, true);
    } catch (const std::exception& ex) { err = ex.what(); return false; }

    if (!root.is_object() || !root.contains("Main") || !root["Main"].is_object()) {
        err = "invalid schema (missing Main)";
        return false;
    }
    const json& main = root["Main"];
    if (!main.contains("Controls") || !main["Controls"].is_array()) {
        err = "invalid schema (missing Controls)";
        return false;
    }

    // build FanCurves index
    std::unordered_map<std::string, json> curvesByName;
    if (main.contains("FanCurves") && main["FanCurves"].is_array()) {
        for (const auto& cj : main["FanCurves"]) {
            if (cj.is_object() && cj.contains("Name") && cj["Name"].is_string())
                curvesByName[cj["Name"].get<std::string>()] = cj;
        }
    }

    out.name = basename_noext(path);

    for (const auto& ctrl : main["Controls"]) {
        if (!ctrl.is_object()) continue;
        if (ctrl.contains("Enable") && ctrl["Enable"].is_boolean() && !ctrl["Enable"].get<bool>()) continue;

        if (!ctrl.contains("Identifier") || !ctrl["Identifier"].is_string()) continue;
        std::string pwmPath = mapPwmFromIdentifier(pwms, ctrl["Identifier"].get<std::string>());
        if (pwmPath.empty()) continue;

        Rule rule{};
        rule.pwmPath = std::move(pwmPath);
        rule.mixFn = MixFunction::Max;

        bool isMix = false;
        std::vector<json> selectedCurves;

        if (ctrl.contains("SelectedFanCurves") && ctrl["SelectedFanCurves"].is_array()) {
            for (const auto& c : ctrl["SelectedFanCurves"]) {
                if (c.is_object() && c.contains("Name") && c["Name"].is_string()) {
                    auto it = curvesByName.find(c["Name"].get<std::string>());
                    if (it != curvesByName.end()) selectedCurves.push_back(it->second);
                }
            }
            if (!selectedCurves.empty()) isMix = true;
        }

        if (!isMix && ctrl.contains("SelectedFanCurve") && ctrl["SelectedFanCurve"].is_object()) {
            const auto& sc = ctrl["SelectedFanCurve"];
            if (sc.contains("Name") && sc["Name"].is_string()) {
                auto it = curvesByName.find(sc["Name"].get<std::string>());
                if (it != curvesByName.end()) selectedCurves.push_back(it->second);
            }
        }

        if (isMix) {
            if (ctrl.contains("SelectedMixFunction") && ctrl["SelectedMixFunction"].is_number_integer())
                rule.mixFn = mix_from_int(ctrl["SelectedMixFunction"].get<int>());

            for (const auto& cj : selectedCurves) {
                std::vector<std::string> tpaths;
                std::vector<std::pair<double,int>> pts;
                int minP=0, maxP=100, spinP=0, spinMs=0; double hyst=0.0; bool stopMin=false;
                std::string e2;
                if (!buildSourceFromCurveJson(cj, temps, tpaths, pts, minP, maxP, hyst, spinP, spinMs, stopMin, e2)) {
                    continue;
                }
                SourceCurve sc;
                sc.label = cj.contains("Name") && cj["Name"].is_string() ? cj["Name"].get<std::string>() : std::string{};
                const bool isTrig = (cj.contains("IdleFanSpeed") && cj.contains("LoadFanSpeed") &&
                                     cj.contains("IdleTemperature") && cj.contains("LoadTemperature"));
                sc.kind = isTrig ? SourceKind::Trigger : SourceKind::Table;
                sc.settings.minPercent   = minP;
                sc.settings.maxPercent   = maxP;
                sc.settings.hysteresisC  = hyst;
                sc.settings.spinupPercent= spinP;
                sc.settings.spinupMs     = spinMs;
                sc.settings.stopBelowMin = stopMin;
                for (auto& pnt : pts) sc.points.push_back({pnt.first, pnt.second});
                sc.tempPaths = std::move(tpaths);
                if (!sc.points.empty()) rule.sources.push_back(std::move(sc));
            }
        } else {
            if (!selectedCurves.empty()) {
                const auto& cj = selectedCurves.front();
                std::vector<std::string> tpaths;
                std::vector<std::pair<double,int>> pts;
                int minP=0, maxP=100, spinP=0, spinMs=0; double hyst=0.0; bool stopMin=false;
                std::string e2;
                if (buildSourceFromCurveJson(cj, temps, tpaths, pts, minP, maxP, hyst, spinP, spinMs, stopMin, e2)) {
                    SourceCurve sc;
                    sc.label = cj.contains("Name") && cj["Name"].is_string() ? cj["Name"].get<std::string>() : std::string{};
                    const bool isTrig = (cj.contains("IdleFanSpeed") && cj.contains("LoadFanSpeed") &&
                                         cj.contains("IdleTemperature") && cj.contains("LoadTemperature"));
                    sc.kind = isTrig ? SourceKind::Trigger : SourceKind::Table;
                    sc.settings.minPercent   = minP;
                    sc.settings.maxPercent   = maxP;
                    sc.settings.hysteresisC  = hyst;
                    sc.settings.spinupPercent= spinP;
                    sc.settings.spinupMs     = spinMs;
                    sc.settings.stopBelowMin = stopMin;
                    for (auto& pnt : pts) sc.points.push_back({pnt.first, pnt.second});
                    sc.tempPaths = std::move(tpaths);
                    if (!sc.points.empty()) rule.sources.push_back(std::move(sc));
                    rule.mixFn = MixFunction::Max;
                }
            }
        }

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
