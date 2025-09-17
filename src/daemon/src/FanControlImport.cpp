/*
 * Linux Fan Control — FanControl.Releases importer (implementation)
 * (c) 2025 LinuxFanControl contributors
 */
#include "FanControlImport.hpp"
#include "Profile.hpp"
#include "Hwmon.hpp"

#include <nlohmann/json.hpp>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <map>
#include <filesystem>

using nlohmann::json;

namespace lfc {

// ---- helpers ----

std::string FanControlImport::lower(std::string s) {
    for (auto& c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}

static int tail_number(const std::string& s) {
    int v = -1;
    for (size_t i = s.size(); i-- > 0; ) {
        if (!std::isdigit((unsigned char)s[i])) {
            if (i + 1 < s.size()) { try { v = std::stoi(s.substr(i + 1)); } catch (...) {} }
            break;
        }
        if (i == 0) { try { v = std::stoi(s); } catch (...) {} }
    }
    return v;
}

std::string FanControlImport::mapPwmFromIdentifier(const std::vector<HwmonPwm>& pwms,
                                                   const std::string& identifier)
{
    int idx = tail_number(identifier);
    if (idx >= 0) ++idx; // control/<n> -> pwm(n+1)
    std::string want = idx > 0 ? ("pwm" + std::to_string(idx)) : std::string();
    for (const auto& p : pwms) {
        auto base = std::filesystem::path(p.path_pwm).filename().string();
        if (!want.empty() && base == want) return p.path_pwm;
    }
    return pwms.empty() ? std::string() : pwms.front().path_pwm;
}

std::string FanControlImport::mapBestTempFromIdentifier(const std::vector<HwmonTemp>& temps,
                                                        const std::string& identifier)
{
    int idx = tail_number(identifier);
    std::string want = idx > 0 ? ("temp" + std::to_string(idx) + "_input") : std::string();

    for (const auto& t : temps) {
        auto base = std::filesystem::path(t.path_input).filename().string();
        if (!want.empty() && base == want) return t.path_input;
    }
    return temps.empty() ? std::string() : temps.front().path_input;
}

void FanControlImport::parsePointsArray(const json& arr, std::vector<std::pair<double,int>>& dst) {
    dst.clear();
    if (!arr.is_array()) return;
    for (const auto& v : arr) {
        if (v.is_string()) {
            auto s = v.get<std::string>();
            auto pos = s.find(',');
            if (pos == std::string::npos) continue;
            try {
                double tc = std::stod(s.substr(0, pos));
                int pct = std::stoi(s.substr(pos + 1));
                dst.emplace_back(tc, pct);
            } catch (...) {}
        } else if (v.is_array() && v.size() >= 2) {
            try {
                double tc = v[0].get<double>();
                int pct = v[1].get<int>();
                dst.emplace_back(tc, pct);
            } catch (...) {}
        }
    }
    std::sort(dst.begin(), dst.end(), [](auto& a, auto& b){ return a.first < b.first; });
}

void FanControlImport::buildTriggerPoints(double idleTemp, int idlePct,
                                          double loadTemp, int loadPct,
                                          std::vector<std::pair<double,int>>& dst)
{
    dst.clear();
    if (loadTemp < idleTemp) { std::swap(loadTemp, idleTemp); std::swap(loadPct, idlePct); }
    dst.emplace_back(idleTemp, idlePct);
    dst.emplace_back(loadTemp, loadPct);
}

bool FanControlImport::buildSourceFromCurveJson(const json& curve,
                                                const std::vector<HwmonTemp>& temps,
                                                std::vector<std::string>& outTempPaths,
                                                std::vector<std::pair<double,int>>& outPoints,
                                                int& outMinPercent, int& outMaxPercent, double& outHystC,
                                                int& outSpinupPercent, int& outSpinupMs,
                                                bool& outStopBelowMin,
                                                bool   isTrigger,
                                                std::string& err)
{
    (void)err; // reserved for future detailed errors

    outTempPaths.clear(); outPoints.clear();
    outMinPercent = 0; outMaxPercent = 100; outHystC = 0.0;
    outSpinupPercent = 0; outSpinupMs = 0; outStopBelowMin = false;

    std::string srcPath;
    if (curve.contains("SelectedTempSource") && curve["SelectedTempSource"].is_object()) {
        auto id = curve["SelectedTempSource"].value("Identifier", std::string());
        if (!id.empty()) srcPath = mapBestTempFromIdentifier(temps, id);
    }
    if (!srcPath.empty()) outTempPaths.push_back(srcPath);

    if (isTrigger) {
        double idleT = curve.value("IdleTemperature", 0.0);
        double loadT = curve.value("LoadTemperature", 0.0);
        int    idleP = curve.value("IdleFanSpeed", 0);
        int    loadP = curve.value("LoadFanSpeed", 0);
        buildTriggerPoints(idleT, idleP, loadT, loadP, outPoints);
        if (curve.contains("ResponseTimeConfig")) {
            const auto& rt = curve["ResponseTimeConfig"];
            int up = rt.value("ResponseTimeUp", 0);
            int dn = rt.value("ResponseTimeDown", 0);
            (void)up; (void)dn;
        }
        return true;
    }

    if (curve.contains("Points")) parsePointsArray(curve["Points"], outPoints);

    if (curve.contains("MaximumCommand")) outMaxPercent = std::clamp(curve["MaximumCommand"].get<int>(), 0, 100);
    if (curve.contains("MinimumCommand")) outMinPercent = std::clamp(curve["MinimumCommand"].get<int>(), 0, 100);
    if (curve.contains("HysteresisConfig")) {
        const auto& h = curve["HysteresisConfig"];
        if (h.contains("HysteresisValueUp"))   outHystC = std::max(outHystC, h["HysteresisValueUp"].get<double>());
        if (h.contains("HysteresisValueDown")) outHystC = std::max(outHystC, h["HysteresisValueDown"].get<double>());
    }
    return !outPoints.empty();
}

bool FanControlImport::LoadAndMap(const std::string& path,
                                  const std::vector<HwmonTemp>& temps,
                                  const std::vector<HwmonPwm>& pwms,
                                  Profile& out,
                                  std::string& err)
{
    err.clear();
    std::ifstream f(path);
    if (!f) { err = "cannot open file"; return false; }
    json root = json::parse(f, nullptr, false);
    if (root.is_discarded()) { err = "invalid JSON"; return false; }

    if (!root.contains("__VERSION__") || !root.contains("Main")) { err = "unexpected schema"; return false; }
    const auto& main = root["Main"];
    if (!main.contains("Controls") || !main["Controls"].is_array()) { err = "missing Controls"; return false; }
    const auto& controls = main["Controls"];
    const auto& curves   = main.contains("FanCurves") ? main["FanCurves"] : json::array();

    std::map<std::string, json> byName;
    for (const auto& c : curves)
        if (c.contains("Name") && c["Name"].is_string()) byName[c["Name"].get<std::string>()] = c;

    out = Profile{};

    for (const auto& ctrl : controls) {
        if (!ctrl.value("Enable", false)) continue;
        if (!ctrl.contains("SelectedFanCurve") || !ctrl["SelectedFanCurve"].is_object()) continue;
        std::string curveName = ctrl["SelectedFanCurve"].value("Name", std::string());
        if (curveName.empty()) continue;

        std::string ident = ctrl.value("Identifier", std::string());
        std::string pwmPath = mapPwmFromIdentifier(pwms, ident);
        if (pwmPath.empty()) continue;

        Rule rule;
        rule.pwmPath = pwmPath;
        rule.mixFn   = MixFunction::Max; // default

        const json* cobj = nullptr;
        auto it = byName.find(curveName);
        if (it != byName.end()) cobj = &it->second;
        if (!cobj) continue;

        int cmdMode = cobj->value("CommandMode", 0); (void)cmdMode;

        bool isMix = (cobj->contains("SelectedFanCurves") && (*cobj)["SelectedFanCurves"].is_array());
        bool isTrigger = (!isMix && cobj->contains("IdleFanSpeed") && cobj->contains("LoadFanSpeed"));

        if (isMix) {
            // Map mix function: only Max/Avg supported; default to Max for unknowns.
            if (cobj->contains("SelectedMixFunction") && (*cobj)["SelectedMixFunction"].is_number_integer()) {
                int fn = (*cobj)["SelectedMixFunction"].get<int>();
                rule.mixFn = (fn == 1) ? MixFunction::Avg : MixFunction::Max;
            }

            for (const auto& ref : (*cobj)["SelectedFanCurves"]) {
                if (!ref.is_object()) continue;
                std::string subName = ref.value("Name", std::string());
                auto it2 = byName.find(subName);
                if (it2 == byName.end()) continue;

                std::vector<std::string> paths;
                std::vector<std::pair<double,int>> pts;
                int minP, maxP, suP, suMs; double hyst; bool stopMin;
                if (!buildSourceFromCurveJson(it2->second, temps, paths, pts, minP, maxP, hyst, suP, suMs, stopMin,
                                              /*isTrigger*/ false, err))
                    continue;

                std::sort(paths.begin(), paths.end());
                paths.erase(std::unique(paths.begin(), paths.end()), paths.end());

                SourceCurve sc;
                sc.kind = SourceKind::Table;
                sc.label = subName;
                sc.tempPaths = paths;
                for (auto& p : pts) sc.points.push_back({p.first, p.second});
                sc.settings.minPercent = minP;
                sc.settings.maxPercent = maxP;
                sc.settings.hysteresisC = hyst;
                sc.settings.spinupPercent = suP;
                sc.settings.spinupMs = suMs;
                sc.settings.stopBelowMin = stopMin;
                rule.sources.push_back(sc);
            }
        } else {
            std::vector<std::string> paths;
            std::vector<std::pair<double,int>> pts;
            int minP, maxP, suP, suMs; double hyst; bool stopMin;
            if (!buildSourceFromCurveJson(*cobj, temps, paths, pts, minP, maxP, hyst, suP, suMs, stopMin,
                                          isTrigger, err))
                continue;

            if (isTrigger && !paths.empty()) {
                std::string best = paths.front();
                paths.clear(); paths.push_back(best);
            }

            std::sort(paths.begin(), paths.end());
            paths.erase(std::unique(paths.begin(), paths.end()), paths.end());

            SourceCurve sc;
            sc.kind = isTrigger ? SourceKind::Trigger : SourceKind::Table;
            sc.label = curveName;
            sc.tempPaths = paths;
            for (auto& p : pts) sc.points.push_back({p.first, p.second});
            sc.settings.minPercent = minP;
            sc.settings.maxPercent = maxP;
            sc.settings.hysteresisC = hyst;
            sc.settings.spinupPercent = suP;
            sc.settings.spinupMs = suMs;
            sc.settings.stopBelowMin = stopMin;
            rule.sources.push_back(sc);
        }

        if (!rule.sources.empty())
            out.rules.push_back(rule);
    }

    if (out.rules.empty()) { err = "no enabled controls mapped"; return false; }
    return true;
}

} // namespace lfc
