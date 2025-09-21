/*
 * Linux Fan Control — FanControl.Releases importer (implementation)
 * Maps upstream JSON schema to our Profile (mix/trigger/graph)
 * (c) 2025 LinuxFanControl contributors
 */
#include "include/FanControlImport.hpp"
#include "include/Hwmon.hpp"
#include "include/Profile.hpp"

#include <nlohmann/json.hpp>
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <optional>
#include <set>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace lfc {

using json = nlohmann::json;
namespace fs = std::filesystem;

static std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c){ return (char)std::tolower(c); });
    return s;
}

static std::string bestOf(const std::string& a, const std::string& b) {
    return a.empty() ? b : a;
}

static std::string readFileToString_(const std::string& path, std::string& err) {
    std::ifstream ifs(path, std::ios::in | std::ios::binary);
    if (!ifs) { err = "cannot open file: " + path; return {}; }
    std::ostringstream oss; oss << ifs.rdbuf(); return oss.str();
}

static int tail_number(const std::string& s) {
    int i = (int)s.size() - 1;
    while (i >= 0 && !std::isdigit((unsigned char)s[(size_t)i])) --i;
    if (i < 0) return -1;
    int j = i;
    while (j >= 0 && std::isdigit((unsigned char)s[(size_t)j])) --j;
    try { return std::stoi(s.substr((size_t)j + 1, (size_t)(i - j))); }
    catch (...) { return -1; }
}

static std::string mapPwmFromIdentifier(const std::vector<HwmonPwm>& pwms,
                                        const std::string& identifier)
{
    const std::string id = lower(identifier);
    int idx = tail_number(id);
    if (idx >= 0) {
        std::string want = "pwm" + std::to_string(idx + 1);
        for (const auto& p : pwms) {
            std::string base = fs::path(p.path_pwm).filename().string();
            if (lower(base) == want) return p.path_pwm;
        }
    }
    for (const auto& p : pwms) {
        std::string base = lower(fs::path(p.path_pwm).filename().string());
        std::string full = lower(p.path_pwm);
        if (id == base || id == full) return p.path_pwm;
    }
    for (const auto& p : pwms) {
        std::string base = lower(fs::path(p.path_pwm).filename().string());
        std::string full = lower(p.path_pwm);
        if (!id.empty() && (base.find(id) != std::string::npos || full.find(id) != std::string::npos))
            return p.path_pwm;
    }
    return {};
}

static std::string mapBestTempFromIdentifier(const std::vector<HwmonTemp>& temps,
                                             const std::string& identifier)
{
    const std::string id = lower(identifier);
    int idx = tail_number(id);
    if (idx >= 0) {
        std::string want = "temp" + std::to_string(idx + 1) + "_input";
        for (const auto& t : temps) {
            std::string base = fs::path(t.path_input).filename().string();
            if (lower(base) == want) return t.path_input;
        }
    }
    for (const auto& t : temps) {
        std::string base = lower(fs::path(t.path_input).filename().string());
        std::string full = lower(t.path_input);
        std::string lbl  = lower(t.label);
        if (id == base || id == full || (!lbl.empty() && id == lbl)) return t.path_input;
    }
    for (const auto& t : temps) {
        std::string base = lower(fs::path(t.path_input).filename().string());
        std::string full = lower(t.path_input);
        std::string lbl  = lower(t.label);
        if (!id.empty() && (base.find(id)!=std::string::npos || full.find(id)!=std::string::npos || (!lbl.empty() && lbl.find(id)!=std::string::npos)))
            return t.path_input;
    }
    return {};
}

static void parsePointsArray_(const json& arr, std::vector<std::pair<double,int>>& dst) {
    dst.clear();
    if (!arr.is_array()) return;
    for (const auto& pt : arr) {
        double tC = 0.0;
        int pct   = 0;
        if (pt.contains("t"))         tC = pt.at("t").get<double>();
        else if (pt.contains("temp")) tC = pt.at("temp").get<double>();
        else if (pt.contains("x"))    tC = pt.at("x").get<double>();
        else if (pt.contains("Temperature") || pt.contains("TemperatureC")) {
            tC = pt.value("TemperatureC", pt.value("Temperature", 0.0));
        }

        if (pt.contains("p"))            pct = pt.at("p").get<int>();
        else if (pt.contains("percent")) pct = pt.at("percent").get<int>();
        else if (pt.contains("y"))       pct = pt.at("y").get<int>();
        else if (pt.contains("Value") || pt.contains("Percent")) {
            pct = pt.value("Percent", pt.value("Value", 0));
        }

        if (pct < 0) pct = 0;
        if (pct > 100) pct = 100;
        dst.emplace_back(tC, pct);
    }
}

static bool buildSourceFromCurveJson_(const json& curve,
                                      const std::vector<HwmonTemp>& temps,
                                      std::vector<std::string>& tempPathsOut,
                                      std::vector<std::pair<double,int>>& ptsOut,
                                      int& minPct, int& maxPct, double& hystC,
                                      int& spinPct, int& spinMs, bool& stopBelowMin,
                                      bool isTrigger, std::string& errOut)
{
    tempPathsOut.clear();
    ptsOut.clear();
    minPct = 0; maxPct = 100; hystC = 0.0; spinPct = 0; spinMs = 0; stopBelowMin = false;

    if (!curve.is_object()) return true;

    if (curve.contains("Temps") && curve["Temps"].is_array()) {
        for (const auto& t : curve["Temps"]) {
            std::string id;
            if (t.is_string()) id = t.get<std::string>();
            else if (t.contains("Id")) id = t.value("Id", std::string{});
            else if (t.contains("Name")) id = t.value("Name", std::string{});
            if (!id.empty()) {
                std::string path = mapBestTempFromIdentifier(temps, id);
                if (!path.empty()) tempPathsOut.push_back(path);
            }
        }
    } else if (curve.contains("Temp") && curve["Temp"].is_string()) {
        std::string id = curve.value("Temp", std::string{});
        std::string path = mapBestTempFromIdentifier(temps, id);
        if (!path.empty()) tempPathsOut.push_back(path);
    }

    if (curve.contains("Points")) {
        parsePointsArray_(curve["Points"], ptsOut);
    } else if (curve.contains("Table")) {
        parsePointsArray_(curve["Table"], ptsOut);
    }

    minPct = curve.value("Minimum", curve.value("Min", 0));
    maxPct = curve.value("Maximum", curve.value("Max", 100));
    if (minPct < 0) minPct = 0;
    if (maxPct > 100) maxPct = 100;
    if (minPct > maxPct) std::swap(minPct, maxPct);

    hystC   = curve.value("Hysteresis", curve.value("HysteresisC", 0.0));
    spinPct = curve.value("SpinUpValue", curve.value("SpinUpPercent", 0));
    spinMs  = curve.value("SpinUpTimeMs", curve.value("SpinUpTime", 0));
    stopBelowMin = curve.value("StopBelowMin", curve.value("StopFanControlBelowMin", false));

    if (!isTrigger && ptsOut.empty() && tempPathsOut.empty()) {
        errOut = "curve without points and temps";
        return false;
    }
    return true;
}

bool FanControlImport::LoadAndMap(const std::string& path,
                                  const std::vector<HwmonTemp>& temps,
                                  const std::vector<HwmonPwm>& pwms,
                                  Profile& out,
                                  std::string& err,
                                  std::function<void(int, const std::string&)> onProgress)
{
    auto progress = [&](int pct, const std::string& msg){
        if (onProgress) onProgress(pct, msg);
    };

    progress(0, "read");
    std::string text = readFileToString_(path, err);
    if (text.empty() && !err.empty()) return false;

    json root = json::parse(text, nullptr, false);
    if (root.is_discarded()) { err = "invalid json"; return false; }

    out = Profile{};
    out.schema = "LinuxFanControl.Profile/v1";

    const json& controls = root.contains("Controls") ? root["Controls"] :
                           (root.contains("controllers") ? root["controllers"] : json::array());
    if (!controls.is_array() || controls.empty()) {
        err = "no Controls in source config";
        return false;
    }

    progress(10, "map.controls");

    auto uniqCurveName = [&](const std::string& base)->std::string{
        static std::unordered_map<std::string, int> used;
        std::string key = lower(base);
        int& n = used[key];
        if (n == 0 && out.fanCurves.end() == std::find_if(out.fanCurves.begin(), out.fanCurves.end(),
            [&](const FanCurveMeta& f){ return lower(f.name) == key; })) {
            n = 1; return base;
        }
        ++n;
        return base + "-" + std::to_string(n);
    };

    int idx = 0;
    for (const auto& ctrl : controls) {
        ++idx;

        std::string pwmIdent =
            ctrl.value("Pwm", ctrl.value("Fan", ctrl.value("AffectFan", ctrl.value("PwmId", std::string{}))));
        if (pwmIdent.empty() && ctrl.contains("Output")) pwmIdent = ctrl["Output"].value("Name", "");
        std::string pwmPath = mapPwmFromIdentifier(pwms, pwmIdent);
        if (pwmPath.empty()) {
            if (ctrl.contains("pwmPath") && ctrl["pwmPath"].is_string())
                pwmPath = ctrl.value("pwmPath", "");
        }
        if (pwmPath.empty()) {
            continue;
        }

        const json& curve = ctrl.contains("Curve") ? ctrl["Curve"] :
                            (ctrl.contains("curve") ? ctrl["curve"] : json::object());
        const std::string typeRaw = curve.value("Type", std::string{});
        const std::string typeL   = lower(typeRaw);
        const bool isTrigger = (typeL == "trigger");

        std::vector<std::string> tempPaths;
        std::vector<std::pair<double,int>> pts;
        int minPct=0, maxPct=100, spinPct=0, spinMs=0; double hyst=0.0; bool stopBelow=false;

        if (!curve.is_null()) {
            std::string perr;
            if (!buildSourceFromCurveJson_(curve, temps, tempPaths, pts,
                                           minPct, maxPct, hyst, spinPct, spinMs, stopBelow,
                                           isTrigger, perr)) { err = perr; return false; }
        } else {
            std::string tId;
            if (ctrl.contains("Temp")) tId = ctrl.at("Temp").get<std::string>();
            if (!tId.empty()) {
                std::string tPath = mapBestTempFromIdentifier(temps, tId);
                if (!tPath.empty()) tempPaths.push_back(tPath);
            }
            pts.emplace_back(40.0, 20);
            pts.emplace_back(80.0, 100);
        }

        MixFunction mixFn = MixFunction::Avg;
        std::string mixStr = curve.value("Function", curve.value("Mix", std::string{}));
        if (mixStr.empty()) mixStr = ctrl.value("Function", ctrl.value("Mix", std::string{}));
        if (!mixStr.empty()) {
            std::string ml = lower(mixStr);
            if (ml == "min")      mixFn = MixFunction::Min;
            else if (ml == "avg") mixFn = MixFunction::Avg;
            else                  mixFn = MixFunction::Max;
        } else if (isTrigger) {
            mixFn = MixFunction::Max;
        }

        FanCurveMeta fcm;
        std::string curveRef = ctrl.value("Name", pwmIdent);
        if (curve.contains("Name")) curveRef = bestOf(curve.value("Name", std::string{}), curveRef);
        if (curveRef.empty()) curveRef = "curve-" + std::to_string(idx);
        fcm.name = uniqCurveName(curveRef);

        if (isTrigger) {
            fcm.type = "trigger";
        } else if (!pts.empty()) {
            fcm.type = "graph";
        } else {
            fcm.type = "mix";
        }
        fcm.mix = mixFn;
        fcm.tempSensors = tempPaths;

        if (fcm.type == "graph") {
            fcm.points.reserve(pts.size());
            for (auto& p : pts) fcm.points.push_back(CurvePoint{p.first, p.second});
        } else if (fcm.type == "trigger") {
            fcm.onC  = curve.value("OnC",  curve.value("On",  0.0));
            fcm.offC = curve.value("OffC", curve.value("Off", 0.0));
        }

        out.fanCurves.push_back(fcm);

        ControlMeta cm;
        cm.name    = ctrl.value("Name", pwmIdent);
        cm.pwmPath = pwmPath;
        cm.curveRef= fcm.name;
        out.controls.push_back(cm);

        progress(10 + (int)((double)idx / std::max<size_t>(controls.size(),1) * 70.0), "map.curve");
        (void)minPct; (void)maxPct; (void)hyst; (void)spinPct; (void)spinMs; (void)stopBelow;
    }

    if (out.controls.empty()) { err = "no controls mapped"; return false; }

    try {
        std::unordered_set<std::string> chips;
        for (const auto& p : pwms) {
            fs::path pp = fs::path(p.path_pwm).parent_path().parent_path();
            fs::path chip = pp.parent_path();
            std::string chipPath = chip.string();
            if (!chipPath.empty() && chips.insert(chipPath).second) {
                HwmonDeviceMeta m{};
                m.hwmonPath = chipPath;
                m.name   = Hwmon::chipNameForPath(chipPath);
                m.vendor = Hwmon::chipVendorForName(m.name);
                out.hwmons.push_back(m);
            }
        }
    } catch (...) {}

    progress(90, "finalize");

    if (out.name.empty()) {
        out.name = fs::path(path).stem().string();
        if (out.name.empty()) out.name = "Imported";
    }

    progress(100, "ok");
    return true;
}

} // namespace lfc
