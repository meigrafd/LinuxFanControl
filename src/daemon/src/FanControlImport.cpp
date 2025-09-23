/*
 * Linux Fan Control — FanControl (Rem0o) profile importer
 * (c) 2025 LinuxFanControl contributors
 */

#include "FanControlImport.hpp"
#include "Utils.hpp"
#include "Log.hpp"
#include "Hwmon.hpp"
#include "Profile.hpp"
#include "VendorMapping.hpp"
#include "GpuMonitor.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <optional>
#include <regex>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include <nlohmann/json.hpp>

using nlohmann::json;

namespace lfc {

/* ============================== string utils ============================== */

static inline std::string trim(std::string s) {
    util::trim(s);
    return s;
}
static inline std::string to_lower(std::string v) {
    for (auto& c : v) c = (char)std::tolower((unsigned char)c);
    return v;
}
static inline bool ieq(const std::string& a, const std::string& b) { return to_lower(a) == to_lower(b); }
static inline bool icontains(const std::string& hay, const std::string& needle) {
    auto h = to_lower(hay), n = to_lower(needle);
    return h.find(n) != std::string::npos;
}
static inline int s2i(const std::string& s, int def = 0) {
    try { size_t pos = 0; int v = std::stoi(trim(s), &pos, 10); (void)pos; return v; }
    catch (...) { return def; }
}
static inline double s2d(const std::string& s, double def = 0.0) {
    try { size_t pos = 0; double v = std::stod(trim(s), &pos); (void)pos; return v; }
    catch (...) { return def; }
}

/* ============================== json utils ================================ */

static inline int j2i(const json& v, int def = 0) {
    try {
        if (v.is_number_integer())   return (int)v.get<long long>();
        if (v.is_number_unsigned())  return (int)v.get<unsigned long long>();
        if (v.is_number_float())     return (int)std::lround(v.get<double>());
        if (v.is_boolean())          return v.get<bool>() ? 1 : 0;
        if (v.is_string())           return s2i(v.get<std::string>(), def);
    } catch (...) {}
    return def;
}
static inline double j2d(const json& v, double def = 0.0) {
    try {
        if (v.is_number_float())     return v.get<double>();
        if (v.is_number_integer())   return (double)v.get<long long>();
        if (v.is_number_unsigned())  return (double)v.get<unsigned long long>();
        if (v.is_boolean())          return v.get<bool>() ? 1.0 : 0.0;
        if (v.is_string())           return s2d(v.get<std::string>(), def);
    } catch (...) {}
    return def;
}
static inline std::string j2s(const json& v, const std::string& def = {}) {
    try {
        if (v.is_string()) return v.get<std::string>();
        if (v.is_number_integer())  return std::to_string(v.get<long long>());
        if (v.is_number_unsigned()) return std::to_string(v.get<unsigned long long>());
        if (v.is_number_float())    return std::to_string(v.get<double>());
        if (v.is_boolean())         return v.get<bool>() ? "true" : "false";
    } catch (...) {}
    return def;
}

/* ============================ identifier parse ============================ */

struct ParsedIdentifier {
    std::string raw;
    std::string token;    // chip token (e.g. nct6799d) or vendor hint
    int         index{-1};
    bool        isTemp{false};
    bool        isCtrl{false};
    bool        isGpu{false};
    bool        isHotspot{false};
    std::string vendorHint; // "amd" | "nvidia" | "intel" (best-effort)
};

static ParsedIdentifier parseIdentifier(const std::string& id)
{
    ParsedIdentifier p{};
    p.raw = id;
    const std::string lid = to_lower(id);

    // /lpc/<chip>/temperature/<idx>
    if (lid.rfind("/lpc/", 0) == 0) {
        p.isTemp = true;
        auto after = lid.substr(5);
        auto slash = after.find('/');
        if (slash != std::string::npos)
            p.token = after.substr(0, slash);
        auto posT = lid.find("/temperature/");
        if (posT != std::string::npos) {
            const std::string tail = trim(lid.substr(posT + 13));
            p.index = s2i(tail, -1);
        }
    }

    // /control/<idx> (+ optional chip token from /lpc/<chip>)
    if (lid.find("/control/") != std::string::npos) {
        p.isCtrl = true;

        if (p.token.empty()) {
            const auto posLpc = lid.find("/lpc/");
            if (posLpc != std::string::npos) {
                auto after = lid.substr(posLpc + 5);
                auto slash = after.find('/');
                if (slash != std::string::npos)
                    p.token = after.substr(0, slash);
            }
        }
        auto posC = lid.rfind("/control/");
        if (posC != std::string::npos) {
            const std::string tail = trim(lid.substr(posC + 9));
            p.index = s2i(tail, -1);
        }
    }

    // GPU temperatures (various FanControl export forms)
    if (lid.find("adlx/") != std::string::npos || lid.find("amdsmi/") != std::string::npos || lid.find("/temp/gpu") != std::string::npos) {
        p.isTemp = true; p.isGpu = true; p.vendorHint = "amd";
        p.isHotspot = (lid.find("hotspot") != std::string::npos || lid.find("junction") != std::string::npos);
    }
    if (lid.find("nvapi/") != std::string::npos || lid.find("nvml/") != std::string::npos || lid.find("nvidia") != std::string::npos) {
        p.isTemp = true; p.isGpu = true; p.vendorHint = "nvidia";
        p.isHotspot = (lid.find("hotspot") != std::string::npos || lid.find("junction") != std::string::npos);
    }
    if ((lid.find("intel") != std::string::npos || lid.find("i915") != std::string::npos || lid.find("xe-") != std::string::npos || lid.find("/xe/") != std::string::npos)
        && lid.find("/temp/") != std::string::npos) {
        p.isTemp = true; p.isGpu = true; p.vendorHint = "intel";
        p.isHotspot = (lid.find("hotspot") != std::string::npos || lid.find("junction") != std::string::npos);
    }

    return p;
}

/* ============================== mapping helpers =========================== */

static inline std::vector<std::string> aliasesFor(const std::string& token) {
    std::vector<std::string> aliases;
    if (!token.empty()) {
        aliases = VendorMapping::instance().chipAliasesFor(token);
        if (aliases.empty()) aliases.push_back(token);
    }
    return aliases;
}

static std::string findTempByChipAndIndex(const std::vector<std::string>& chipAliases,
                                          int index0,
                                          const std::vector<HwmonTemp>& temps)
{
    // FanControl indices are often 0-based; hwmon is typically 1-based.
    const std::string t0 = (index0 >= 0) ? ("temp" + std::to_string(index0)     + "_input") : "";
    const std::string t1 = (index0 >= 0) ? ("temp" + std::to_string(index0 + 1) + "_input") : "";

    for (const auto& alias : chipAliases) {
        for (const auto& t : temps) {
            if (!icontains(t.chipPath, alias)) continue;
            if (!t0.empty() && icontains(t.path_input, t0)) return t.path_input;
            if (!t1.empty() && icontains(t.path_input, t1)) return t.path_input;
        }
    }
    return {};
}

static std::string findAnyTempOnChip(const std::vector<std::string>& chipAliases,
                                     const std::vector<HwmonTemp>& temps)
{
    for (const auto& alias : chipAliases)
        for (const auto& t : temps)
            if (icontains(t.chipPath, alias)) return t.path_input;
    return {};
}

/* -------- GPU ID → hwmon temp*_input via VendorMapping + GpuMonitor ------- */

static std::string findGpuTempPathViaGpuMonitor(const std::string& sourceIdentifier)
{
    auto [vendor, kind] = VendorMapping::instance().gpuVendorAndKindFromIdentifier(sourceIdentifier);
    if (vendor == "Unknown" || kind == "Unknown") {
        LOG_DEBUG("import: gpu id not recognized id=%s vendor=%s kind=%s",
                  sourceIdentifier.c_str(), vendor.c_str(), kind.c_str());
        return {};
    }

    auto gpus = GpuMonitor::snapshot();
    LOG_DEBUG("import: gpu snapshot count=%zu (vendor=%s kind=%s)", gpus.size(), vendor.c_str(), kind.c_str());
    for (const auto& g : gpus) {
        LOG_DEBUG("import: gpu #%d vendor=%s pci=%s hwmon=%s", g.index, g.vendor.c_str(), g.pciBusId.c_str(), g.hwmonPath.c_str());
        if (g.vendor != vendor) continue;
        if (g.hwmonPath.empty()) continue;

        std::string path = GpuMonitor::resolveHwmonTempPath(g.hwmonPath, kind); // kind: Edge/Hotspot/Memory
        if (!path.empty()) {
            LOG_DEBUG("import: gpu map via monitor id=%s -> pci=%s kind=%s path=%s",
                      sourceIdentifier.c_str(), g.pciBusId.c_str(), kind.c_str(), path.c_str());
            return path;
        }
    }

    LOG_DEBUG("import: gpu map failed id=%s vendor=%s kind=%s", sourceIdentifier.c_str(), vendor.c_str(), kind.c_str());
    return {};
}

static std::string findBestTempByIdentifier(const ParsedIdentifier& pid,
                                            const std::string& originalIdentifier,
                                            const std::vector<HwmonTemp>& temps)
{
    if (pid.isGpu) {
        const auto path = findGpuTempPathViaGpuMonitor(originalIdentifier);
        if (!path.empty()) return path;

        // Fallback: labeled GPU temperature from hwmon inventory
        for (const auto& t : temps) {
            const auto lchip = to_lower(t.chipPath);
            const bool isGpuChip = icontains(lchip, "amdgpu") || icontains(lchip, "nvidia") || icontains(lchip, "i915") || icontains(lchip, "xe");
            if (!isGpuChip) continue;
            if (pid.isHotspot) {
                if (icontains(to_lower(t.label), "junction") || icontains(to_lower(t.label), "hotspot"))
                    return t.path_input;
            } else {
                if (icontains(to_lower(t.label), "edge") || icontains(to_lower(t.label), "gpu"))
                    return t.path_input;
            }
        }
        for (const auto& t : temps) {
            const auto lchip = to_lower(t.chipPath);
            if (icontains(lchip, "amdgpu") || icontains(lchip, "nvidia") || icontains(lchip, "i915") || icontains(lchip, "xe"))
                return t.path_input;
        }
        return {};
    }

    if (!pid.token.empty() && pid.index >= 0) {
        const auto aliases = aliasesFor(pid.token);
        const auto p = findTempByChipAndIndex(aliases, pid.index, temps);
        if (!p.empty()) return p;
    }

    if (!pid.token.empty()) {
        const auto aliases = aliasesFor(pid.token);
        const auto p = findAnyTempOnChip(aliases, temps);
        if (!p.empty()) return p;
    }

    return {};
}

/* ======================== curve points parser ============================= */

static std::vector<CurvePoint> parseCurvePoints(const json& arr)
{
    std::vector<CurvePoint> pts;
    if (!arr.is_array()) return pts;
    pts.reserve(arr.size());

    for (const auto& p : arr) {
        CurvePoint cp{};
        if (p.is_object()) {
            if (p.contains("Temp") || p.contains("Speed")) {
                cp.tempC   = p.contains("Temp")  ? j2d(p.at("Temp"), 0.0) : 0.0;
                cp.percent = p.contains("Speed") ? std::clamp(j2i(p.at("Speed"), 0), 0, 100) : 0;
            } else {
                cp.tempC   = p.contains("X") ? j2d(p.at("X"), 0.0) : 0.0;
                cp.percent = p.contains("Y") ? std::clamp(j2i(p.at("Y"), 0), 0, 100) : 0;
            }
        } else if (p.is_array() && p.size() >= 2) {
            cp.tempC   = j2d(p.at(0), 0.0);
            cp.percent = std::clamp(j2i(p.at(1), 0), 0, 100);
        }
        pts.push_back(cp);
    }
    return pts;
}

/* ========================== sensor source mapping ========================= */

static void addIdentifierIfResolves(const std::string& src,
                                    std::vector<std::string>& sensors,
                                    const std::vector<HwmonTemp>& temps)
{
    const ParsedIdentifier pid = parseIdentifier(src);
    const std::string path = findBestTempByIdentifier(pid, src, temps);
    if (!path.empty()) {
        LOG_DEBUG("import: temp source '%s' -> '%s'", src.c_str(), path.c_str());
        sensors.push_back(path);
    } else {
        LOG_DEBUG("import: temp source unresolved: %s", src.c_str());
    }
}

static void collectCurveTempSources(const json& c,
                                    std::vector<std::string>& sensors,
                                    const std::vector<HwmonTemp>& temps)
{
    if (c.contains("Temperature") && c["Temperature"].is_object()) {
        const auto& t = c["Temperature"];
        if (t.contains("Identifier") && t["Identifier"].is_string())
            addIdentifierIfResolves(t["Identifier"].get<std::string>(), sensors, temps);
    }
    if (c.contains("Identifier") && c["Identifier"].is_string())
        addIdentifierIfResolves(c["Identifier"].get<std::string>(), sensors, temps);
    if (c.contains("Sensors") && c["Sensors"].is_array()) {
        for (const auto& s : c["Sensors"]) {
            if (s.is_object() && s.contains("Identifier") && s["Identifier"].is_string())
                addIdentifierIfResolves(s["Identifier"].get<std::string>(), sensors, temps);
            else if (s.is_string())
                addIdentifierIfResolves(s.get<std::string>(), sensors, temps);
        }
    }
    if (c.contains("Sensor") && c["Sensor"].is_object()) {
        const auto& s = c["Sensor"];
        if (s.contains("Identifier") && s["Identifier"].is_string())
            addIdentifierIfResolves(s["Identifier"].get<std::string>(), sensors, temps);
    }
    if (c.contains("Source") && c["Source"].is_string())
        addIdentifierIfResolves(c["Source"].get<std::string>(), sensors, temps);
    if (c.contains("TemperatureSource") && c["TemperatureSource"].is_string())
        addIdentifierIfResolves(c["TemperatureSource"].get<std::string>(), sensors, temps);

    std::set<std::string> seen;
    std::vector<std::string> dedup;
    dedup.reserve(sensors.size());
    for (const auto& s : sensors) {
        if (seen.insert(s).second) dedup.push_back(s);
    }
    sensors.swap(dedup);
}

/* ============================== hwmon base utils ========================== */

static std::string hwmonBaseNameOf(const std::string& anyPath) {
    // Extract "hwmonN" from any path under /sys/{class,devices}/.../hwmon/hwmonN/...
    // Returns empty if not found.
    const std::string needle = "/hwmon/";
    auto pos = anyPath.rfind(needle);
    if (pos == std::string::npos) return {};
    auto start = pos + needle.size();
    auto slash = anyPath.find('/', start);
    if (slash == std::string::npos) slash = anyPath.size();
    return anyPath.substr(start, slash - start); // e.g. "hwmon4"
}

/* ================================ import ================================== */

bool FanControlImport::LoadAndMap(const std::string& path,
                                  const std::vector<HwmonTemp>& temps,
                                  const std::vector<HwmonPwm>& pwms,
                                  Profile& outProfile,
                                  std::string& err,
                                  ProgressFn onProgress,
                                  nlohmann::json* detailsOut)
{
    auto progress = [&](int pct, const std::string& msg){ if (onProgress) onProgress(pct, msg); };

    err.clear();
    progress(0, "Reading source profile...");

    std::ifstream ifs(path);
    if (!ifs) { err = "cannot open FanControl file: " + path; return false; }
    json root;
    try { ifs >> root; }
    catch (const std::exception& e) { err = std::string("invalid JSON: ") + e.what(); return false; }

    if (!root.is_object()) { err = "unexpected JSON root"; return false; }

    if (detailsOut) {
        (*detailsOut) = json::object();
        (*detailsOut)["source"] = "FanControl";
    }

    progress(10, "Parsing curves...");

    size_t srcCurveCount = 0;
    size_t addedControls = 0;
    size_t skippedNoCurve = 0;
    size_t skippedDup     = 0;
    size_t pwmMapped      = 0;
    size_t pwmUnmapped    = 0;

    outProfile.fanCurves.clear();
    outProfile.controls.clear();

    if (root.contains("FanCurves") && root["FanCurves"].is_array()) {
        const auto& arr = root["FanCurves"];
        srcCurveCount = arr.size();

        for (const auto& c : arr) {
            if (!c.is_object()) continue;
            std::string name = c.contains("Name") && c["Name"].is_string() ? c["Name"].get<std::string>() : "<unnamed>";

            std::vector<std::string> sources;
            collectCurveTempSources(c, sources, temps);

            std::vector<CurvePoint> pts;
            if (c.contains("Points")) pts = parseCurvePoints(c["Points"]);

            outProfile.fanCurves.emplace_back();
            auto& fc = outProfile.fanCurves.back();
            fc.name = name;
            fc.points.reserve(pts.size());
            for (const auto& p : pts) fc.points.emplace_back(p.tempC, p.percent);
            fc.tempSensors = std::move(sources);
        }
    }

    progress(55, "Mapping controls...");

    // Robust PWM selection: match by hwmon base name (e.g., "hwmon4")
    auto pickPwmOnSameChip = [&](const std::string& curveName)->std::string {
        const auto it = std::find_if(outProfile.fanCurves.begin(), outProfile.fanCurves.end(),
                                     [&](const auto& f){ return f.name == curveName; });
        if (it == outProfile.fanCurves.end() || it->tempSensors.empty()) return {};

        const std::string firstSensor = it->tempSensors.front();
        const std::string sensBase = hwmonBaseNameOf(firstSensor);
        if (sensBase.empty()) {
            // Fallback: first available PWM (keeps project functional)
            if (!pwms.empty()) return pwms.front().path_pwm;
            return {};
        }

        for (const auto& p : pwms) {
            const std::string pwmBase = hwmonBaseNameOf(p.path_pwm);
            if (!pwmBase.empty() && pwmBase == sensBase) {
                LOG_DEBUG("import: pwm matched by hwmon '%s' for curve='%s' -> %s",
                          sensBase.c_str(), curveName.c_str(), p.path_pwm.c_str());
                return p.path_pwm;
            }
        }

        // Last resort: first PWM to avoid losing control creation entirely
        if (!pwms.empty()) {
            LOG_DEBUG("import: pwm fallback (same hwmon not found) curve='%s' -> %s",
                      curveName.c_str(), pwms.front().path_pwm.c_str());
            return pwms.front().path_pwm;
        }
        return {};
    };

    if (root.contains("SelectedFanCurves")) {
        const auto& sfc = root["SelectedFanCurves"];

        if (sfc.is_array()) {
            for (const auto& e : sfc) {
                std::string curveRef = e.is_object() && e.contains("Name") && e["Name"].is_string()
                                     ? e["Name"].get<std::string>()
                                     : (e.is_string() ? e.get<std::string>() : "");
                if (curveRef.empty()) { ++skippedNoCurve; continue; }

                std::string ctrlName, nick;
                if (e.is_object()) {
                    if (e.contains("ControlName") && e["ControlName"].is_string()) ctrlName = e["ControlName"].get<std::string>();
                    if (e.contains("Nickname")    && e["Nickname"].is_string())    nick     = e["Nickname"].get<std::string>();
                }

                ControlMeta cm{};
                cm.name     = nick.empty() ? ctrlName : nick;
                cm.curveRef = curveRef;
                cm.pwmPath  = pickPwmOnSameChip(curveRef);
                if (cm.pwmPath.empty()) ++pwmUnmapped; else ++pwmMapped;

                bool dup = false;
                for (const auto& ex : outProfile.controls)
                    if (cm.name == ex.name && cm.curveRef == ex.curveRef && cm.pwmPath == ex.pwmPath) { dup = true; break; }

                if (dup) { ++skippedDup; continue; }
                if (!cm.curveRef.empty()) { outProfile.controls.push_back(std::move(cm)); ++addedControls; }
                else { ++skippedNoCurve; }
            }
        } else if (sfc.is_object()) {
            std::string curveRef = sfc.contains("Name") && sfc["Name"].is_string() ? sfc["Name"].get<std::string>()
                                : (sfc.contains("SelectedFanCurve") && sfc["SelectedFanCurve"].is_string()
                                  ? sfc["SelectedFanCurve"].get<std::string>()
                                  : "");
            if (!curveRef.empty()) {
                ControlMeta cm{};
                cm.name     = curveRef;
                cm.curveRef = curveRef;
                cm.pwmPath  = pickPwmOnSameChip(curveRef);
                if (cm.pwmPath.empty()) ++pwmUnmapped; else ++pwmMapped;
                outProfile.controls.push_back(std::move(cm));
                ++addedControls;
            } else {
                ++skippedNoCurve;
            }
        }
    }

    progress(85, "Summarizing...");

    if (detailsOut) {
        (*detailsOut)["curves"]         = outProfile.fanCurves.size();
        (*detailsOut)["controls"]       = outProfile.controls.size();
        (*detailsOut)["sourceCurves"]   = srcCurveCount;
        (*detailsOut)["addedControls"]  = addedControls;
        (*detailsOut)["skippedNoCurve"] = skippedNoCurve;
        (*detailsOut)["skippedDup"]     = skippedDup;
        (*detailsOut)["pwmMapped"]      = pwmMapped;
        (*detailsOut)["pwmUnmapped"]    = pwmUnmapped;
    }

    LOG_DEBUG("import: summary — curves=%zu srcCurves=%zu addedControls=%zu skippedNoCurve=%zu skippedDup=%zu pwmMapped=%zu pwmUnmapped=%zu",
              outProfile.fanCurves.size(), srcCurveCount, addedControls, skippedNoCurve, skippedDup, pwmMapped, pwmUnmapped);

    progress(100, "done");
    return true;
}

} // namespace lfc
