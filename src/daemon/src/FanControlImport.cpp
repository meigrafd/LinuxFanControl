/*
 * Linux Fan Control — FanControl (Rem0o) Release Import (implementation)
 * (c) 2025 LinuxFanControl contributors
 *
 * Summary:
 *  - Curve type detection independent of CommandMode:
 *      • MIX      → SelectedFanCurves[] present (+ SelectedMixFunction)
 *      • TRIGGER  → Idle/Load present OR curve name contains "trigger"
 *      • GRAPH    → Points[] present (fallback: 2 points from Idle/Load)
 *  - Sensor mapping (one temp sensor per curve):
 *      • /lpc/<chip>/temperature/<idx>  → hwmon tempN_input (1-based)
 *      • if a /sys/... hwmon path is already given, accept it
 *  - PWM mapping:
 *      • Same-chip heuristic (curve sensor chip ↔ pwm chipPath)
 *      • GPU identifiers → VendorMapping + Hwmon chipName
 *  - Controls with curves that resolve to no effective sensors will be disabled.
 */

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <optional>
#include <regex>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include "include/FanControlImport.hpp"
#include "include/Profile.hpp"
#include "include/Hwmon.hpp"
#include "include/Utils.hpp"
#include "include/Log.hpp"
#include "include/Version.hpp"
#include "include/VendorMapping.hpp"

namespace lfc {
using json = nlohmann::json;

using util::read_json_file;
using util::to_lower;
using util::icontains;
using util::trim;
using util::baseName;

/* --------------------------- small helpers --------------------------- */

static inline std::string hwmonNameOf(const std::string& anyPath) {
    auto pos = anyPath.rfind("/hwmon/");
    if (pos == std::string::npos) return {};
    auto tail = anyPath.substr(pos + 7);
    auto slash = tail.find('/');
    return (slash == std::string::npos) ? tail : tail.substr(0, slash);
}

static inline int tempIndexFromBasename(const std::string& bn) {
    if (bn.rfind("temp", 0) != 0) return -1;
    size_t i = 4, j = i;
    while (j < bn.size() && std::isdigit((unsigned char)bn[j])) ++j;
    if (j == i) return -1;
    return std::atoi(bn.substr(i, j - i).c_str());
}

/* ---------------------------- tolerant JSON --------------------------- */

static inline std::string j2s(const json& j, const char* key, const std::string& def = {}) {
    if (!j.contains(key)) return def;
    const auto& v = j[key];
    try {
        if (v.is_string())          return v.get<std::string>();
        if (v.is_number_integer())  return std::to_string(v.get<long long>());
        if (v.is_number_unsigned()) return std::to_string(v.get<unsigned long long>());
        if (v.is_number_float())    return std::to_string(v.get<double>());
        if (v.is_boolean())         return v.get<bool>() ? "true" : "false";
    } catch (...) {}
    return def;
}
static inline bool j2b(const json& j, const char* key, bool def=false) {
    if (!j.contains(key)) return def;
    const auto& v = j[key];
    try {
        if (v.is_boolean())         return v.get<bool>();
        if (v.is_number_integer())  return v.get<long long>() != 0;
        if (v.is_number_unsigned()) return v.get<unsigned long long>() != 0ULL;
        if (v.is_number_float())    return std::fabs(v.get<double>()) > 0.0;
        if (v.is_string()) {
            const auto s = util::to_lower(v.get<std::string>());
            if (s == "true" || s == "yes" || s == "on" || s == "1") return true;
            if (s == "false"|| s == "no"  || s == "off"|| s == "0") return false;
        }
    } catch (...) {}
    return def;
}
static inline double j2d_any(const json& v, double def=0.0) {
    try {
        if (v.is_number_float())    return v.get<double>();
        if (v.is_number_integer())  return static_cast<double>(v.get<long long>());
        if (v.is_number_unsigned()) return static_cast<double>(v.get<unsigned long long>());
        if (v.is_string())          return std::stod(v.get<std::string>());
        if (v.is_boolean())         return v.get<bool>() ? 1.0 : 0.0;
    } catch (...) {}
    return def;
}
static inline double j2d(const json& j, const char* key, double def=0.0) {
    if (!j.contains(key)) return def;
    return j2d_any(j.at(key), def);
}

/* ----------------------------- points parsing --------------------------- */

static std::vector<CurvePoint> parse_points(const json& pts) {
    std::vector<CurvePoint> out;
    if (!pts.is_array()) return out;
    for (const auto& s : pts) {
        if (s.is_string()) {
            const auto str = s.get<std::string>();
            auto comma = str.find(',');
            if (comma != std::string::npos) {
                try {
                    double t = std::stod(util::trim(str.substr(0, comma)));
                    double p = std::stod(util::trim(str.substr(comma+1)));
                    out.push_back({t, p});
                } catch (...) {}
            }
            continue;
        }
        if (s.is_object()) {
            CurvePoint cp{};
            cp.tempC   = j2d(s, "X", j2d(s, "Temperature", 0.0));
            cp.percent = j2d(s, "Y", j2d(s, "FanSpeed",    0.0));
            out.push_back(cp);
        }
    }
    std::sort(out.begin(), out.end(), [](const auto& a, const auto& b){ return a.tempC < b.tempC; });
    return out;
}

/* ---------------------- identifier → hwmon temp mapping ------------------ */

static bool chipTokenMatches(const std::string& chipPathLower, const std::string& tokenLower) {
    if (chipPathLower.find(tokenLower) != std::string::npos) return true;
    return false;
}

static void addIdentifierIfResolves(const std::string& identifier,
                                    const std::vector<HwmonTemp>& temps,
                                    std::vector<std::string>& outTempPaths)
{
    if (identifier.empty()) return;

    // Already a hwmon path?
    if (identifier.rfind("/sys/", 0) == 0) {
        const bool exists = std::any_of(temps.begin(), temps.end(),
            [&](const HwmonTemp& t){ return t.path_input == identifier; });
        if (exists) outTempPaths.push_back(identifier);
        return;
    }

    const std::string id = util::to_lower(util::trim(identifier));

    // /lpc/<chip-token>/temperature/<idx>  (idx 0-based → tempN_input 1-based)
    {
        static const std::regex re_lpc(R"(^/lpc/([a-z0-9_]+?)/temperature/([0-9]+)$)");
        std::smatch m;
        if (std::regex_match(id, m, re_lpc) && m.size() == 3) {
            const std::string chipTok = m[1].str();
            const int idx0 = std::stoi(m[2].str());
            const int want = idx0 + 1;

            for (const auto& t : temps) {
                const auto chipL = util::to_lower(t.chipPath);
                if (!chipTokenMatches(chipL, chipTok)) continue;
                if (tempIndexFromBasename(util::baseName(t.path_input)) == want) {
                    outTempPaths.push_back(t.path_input);
                }
            }
            if (!outTempPaths.empty()) return;

            // relaxed: any sensor on that chip (last resort)
            for (const auto& t : temps) {
                const auto chipL = util::to_lower(t.chipPath);
                if (chipTokenMatches(chipL, chipTok)) {
                    outTempPaths.push_back(t.path_input);
                }
            }
            if (!outTempPaths.empty()) {
                std::sort(outTempPaths.begin(), outTempPaths.end());
                outTempPaths.erase(std::unique(outTempPaths.begin(), outTempPaths.end()), outTempPaths.end());
                return;
            }
        }
    }

    {
            for (const auto& t : temps) {
                const auto chip = util::to_lower(t.chipPath);
                const auto lab = util::to_lower(t.label);
                if (lab == "tctl" || lab == "tdie" || util::icontains(lab, "cpu"))
                    outTempPaths.push_back(t.path_input);
            }
            if (!outTempPaths.empty()) {
                std::sort(outTempPaths.begin(), outTempPaths.end());
                outTempPaths.erase(std::unique(outTempPaths.begin(), outTempPaths.end()), outTempPaths.end());
                return;
            }
    }

    {
            const bool wantEdge    = util::icontains(id, "/temp/gpu");
            const bool wantHotspot = util::icontains(id, "/temp/hotspot");
            const bool wantMem     = util::icontains(id, "/temp/memory") || util::icontains(id, "/temp/mem");

            for (const auto& t : temps) {
                const std::string chip = util::to_lower(t.chipPath);
                const std::string lbl = util::to_lower(t.label);
                if (wantEdge    && (util::icontains(lbl, "edge")    || util::icontains(lbl, "gpu")))     outTempPaths.push_back(t.path_input);
                if (wantHotspot &&  util::icontains(lbl, "hotspot"))                                outTempPaths.push_back(t.path_input);
                if (wantMem     && (util::icontains(lbl, "mem") ||   util::icontains(lbl, "memory")))    outTempPaths.push_back(t.path_input);
            }
            if (!outTempPaths.empty()) {
                std::sort(outTempPaths.begin(), outTempPaths.end());
                outTempPaths.erase(std::unique(outTempPaths.begin(), outTempPaths.end()), outTempPaths.end());
                return;
            }
    }

    // generic label hints
    auto tryLabel = [&](std::initializer_list<const char*> keys)->bool{
        for (const auto& t : temps) {
            const auto lab = util::to_lower(t.label);
            for (auto* k : keys) if (util::icontains(lab, k)) { outTempPaths.push_back(t.path_input); return true; }
        }
        return false;
    };

    if (util::icontains(id, "hotspot") || util::icontains(id, "junction")) { if (tryLabel({"hotspot","junction"})) return; }
    if (util::icontains(id, "edge") || util::icontains(id, "gpu"))         { if (tryLabel({"edge","gpu"}))       return; }
    if (util::icontains(id, "mem") || util::icontains(id, "memory") || util::icontains(id, "vram")) { if (tryLabel({"mem","memory","vram"})) return; }
    if (util::icontains(id, "ambient"))                               { if (tryLabel({"ambient","systin"})) return; }
    if (util::icontains(id, "water"))                                 { if (tryLabel({"water"}))            return; }
}

/* ------------------------------ curve helpers --------------------------- */

static std::string resolveSingleTemp(const std::string& identifier,
                                     const std::vector<HwmonTemp>& temps)
{
    std::vector<std::string> tmp;
    addIdentifierIfResolves(identifier, temps, tmp);
    if (tmp.empty()) return {};
    std::sort(tmp.begin(), tmp.end());
    tmp.erase(std::unique(tmp.begin(), tmp.end()), tmp.end());
    return tmp.front();
}

static void collectCurveTempSources(const json& curveObj,
                                    std::vector<std::string>& outTempPaths,
                                    const std::vector<HwmonTemp>& temps)
{
    outTempPaths.clear();
    if (curveObj.contains("SelectedTempSource")) {
        const auto& sts = curveObj["SelectedTempSource"];
        std::string ident;
        if (sts.is_string()) ident = sts.get<std::string>();
        else if (sts.is_object() && sts.contains("Identifier") && sts["Identifier"].is_string())
            ident = sts["Identifier"].get<std::string>();
        if (!ident.empty()) {
            const auto one = resolveSingleTemp(ident, temps);
            if (!one.empty()) outTempPaths.push_back(one);
        }
    }
    // optional legacy fallback
    if (outTempPaths.empty() && curveObj.contains("Source") && curveObj["Source"].is_string()) {
        const auto one = resolveSingleTemp(curveObj["Source"].get<std::string>(), temps);
        if (!one.empty()) outTempPaths.push_back(one);
    }
}

// true if curve has effective sensors (for mix: see referenced curves)
static bool curveHasEffectiveSensors(const Profile& prof, const FanCurveMeta& fc) {
    if (fc.type == "graph" || fc.type == "trigger") {
        return !fc.tempSensors.empty();
    }
    if (fc.type == "mix") {
        for (const auto& ref : fc.curveRefs) {
            const FanCurveMeta* sub = nullptr;
            for (const auto& f : prof.fanCurves) if (f.name == ref) { sub = &f; break; }
            if (sub && curveHasEffectiveSensors(prof, *sub)) return true;
        }
    }
    return false;
}

/* --------- after curves parsed: populate MIX tempSensors (union) -------- */

static void populate_mix_sensor_unions(Profile& prof) {
    // Build name → ptr map
    std::unordered_map<std::string, FanCurveMeta*> byName;
    byName.reserve(prof.fanCurves.size());
    for (auto& f : prof.fanCurves) byName[f.name] = &f;

    // Recursive collector
    std::function<void(const FanCurveMeta*, std::unordered_set<std::string>&, std::unordered_set<std::string>&)> collect;
    collect = [&](const FanCurveMeta* cur, std::unordered_set<std::string>& acc, std::unordered_set<std::string>& visiting){
        if (!cur) return;
        if (visiting.count(cur->name)) return; // prevent cycles
        visiting.insert(cur->name);

        if (cur->type == "graph" || cur->type == "trigger") {
            for (const auto& s : cur->tempSensors) acc.insert(s);
        } else if (cur->type == "mix") {
            for (const auto& r : cur->curveRefs) {
                auto it = byName.find(r);
                if (it != byName.end()) collect(it->second, acc, visiting);
            }
        }
    };

    for (auto& f : prof.fanCurves) {
        if (f.type != "mix") continue;
        std::unordered_set<std::string> acc, visiting;
        collect(&f, acc, visiting);
        f.tempSensors.assign(acc.begin(), acc.end());
        std::sort(f.tempSensors.begin(), f.tempSensors.end());
        LOG_DEBUG("import: mix '%s' sensor union size=%zu", f.name.c_str(), f.tempSensors.size());
    }
}

/* ------------------------------ PWM mapping ----------------------------- */

static std::string pickPwmForCurveOnSameChip(const Profile& prof,
                                             const std::vector<HwmonPwm>& pwms,
                                             const std::string& curveName)
{
    const FanCurveMeta* fc = nullptr;
    for (const auto& f : prof.fanCurves) if (f.name == curveName) { fc = &f; break; }
    if (!fc || fc->tempSensors.empty()) return {};

    const std::string want = hwmonNameOf(fc->tempSensors.front());
    if (want.empty()) return {};

    for (const auto& p : pwms) {
        if (hwmonNameOf(p.chipPath) == want && hwmonNameOf(p.path_pwm) == want) {
            return p.path_pwm;
        }
    }
    return {};
}

static std::string map_windows_control_identifier_to_pwm(const std::string& identifier,
                                                         const std::vector<HwmonPwm>& pwms)
{
    if (identifier.empty()) return {};
    const std::string id = util::to_lower(util::trim(identifier));

    // already a hwmon path?
    if (id.rfind("/sys/", 0) == 0) {
        for (const auto& p : pwms) if (p.path_pwm == identifier) return p.path_pwm;
        return identifier;
    }

    // /lpc/<chip-token>/control/<idx>
    {
        static const std::regex re_lpc(R"(^/lpc/([a-z0-9_]+?)/control/([0-9]+)$)");
        std::smatch m;
        if (std::regex_match(id, m, re_lpc) && m.size() == 3) {
            const std::string chipTok = m[1].str();
            const int idx0 = std::stoi(m[2].str());
            if (idx0 >= 0 && idx0 < 64) {
                const std::string want = "pwm" + std::to_string(idx0 + 1);
                std::vector<const HwmonPwm*> cands;
                for (const auto& p : pwms) {
                    if (util::baseName(p.path_pwm) == want) cands.push_back(&p);
                }
                if (!cands.empty()) {
                    const std::string tok = util::to_lower(chipTok);
                    auto score = [&](const HwmonPwm* p){
                        const auto s = util::to_lower(p->chipPath);
                        if (s.find(tok) != std::string::npos) return 2;
                        return 0;
                    };
                    const HwmonPwm* best = cands.front();
                    int bestScore = -1;
                    for (auto* c : cands) { int sc = score(c); if (sc > bestScore) { bestScore = sc; best = c; } }
                    return best->path_pwm;
                }
            }
        }
    }

    {
        auto& vm = VendorMapping::instance();
        const auto pair = vm.gpuVendorAndKindFromIdentifier(id);
        const std::string venCanonical = pair.first;

        if (venCanonical != "Unknown") {
            std::vector<const HwmonPwm*> gpuPwms;
            gpuPwms.reserve(pwms.size());

            for (const auto& p : pwms) {
                const std::string chipName   = util::to_lower(Hwmon::chipNameForPath(p.chipPath));
                const std::string prettyVend = vm.vendorForChipName(chipName);
                const std::string canonVend  = vm.gpuCanonicalVendor(prettyVend);
                if (canonVend != venCanonical) continue;
                gpuPwms.push_back(&p);
            }

            if (!gpuPwms.empty()) {
                auto hasEnable = [](const HwmonPwm* p){ return !p->path_enable.empty(); };
                auto labelHasGpu = [](const HwmonPwm* p){
                    return util::icontains(p->label, "gpu") || util::icontains(p->label, "graphics") || util::icontains(p->label, "vga");
                };
                auto pwmIndex = [](const HwmonPwm* p){
                    const std::string b = util::baseName(p->path_pwm);
                    int n = 999;
                    if (b.rfind("pwm", 0) == 0) {
                        try { n = std::stoi(b.substr(3)); } catch (...) {}
                    }
                    return n;
                };

                std::stable_sort(gpuPwms.begin(), gpuPwms.end(), [&](const HwmonPwm* a, const HwmonPwm* b){
                    const bool ae = hasEnable(a), be = hasEnable(b);
                    if (ae != be) return ae;
                    const bool ag = labelHasGpu(a), bg = labelHasGpu(b);
                    if (ag != bg) return ag;
                    return pwmIndex(a) < pwmIndex(b);
                });

                return gpuPwms.front()->path_pwm;
            }
        }
    }

    return {};
}

/* ------------------------- control deduplication ------------------------- */

static void dedupe_controls_by_pwm(Profile& out) {
    // drop fully hidden+disabled manual orphans early
    {
        std::vector<ControlMeta> keep;
        keep.reserve(out.controls.size());
        for (auto& c : out.controls) {
            if (!c.enabled && c.manual && c.hidden) continue;
            keep.push_back(std::move(c));
        }
        out.controls.swap(keep);
    }

    std::unordered_map<std::string, size_t> bestIdx;
    std::vector<ControlMeta> result;
    result.reserve(out.controls.size());

    auto score = [](const ControlMeta& c)->int {
        int s = 0;
        if (c.enabled) s += 4;
        if (!c.hidden) s += 2;
        if (!c.nickName.empty()) s += 1;
        return s;
    };

    for (size_t i = 0; i < out.controls.size(); ++i) {
        auto& c = out.controls[i];
        if (c.pwmPath.empty()) { result.push_back(std::move(c)); continue; }
        auto it = bestIdx.find(c.pwmPath);
        if (it == bestIdx.end()) {
            bestIdx[c.pwmPath] = result.size();
            result.push_back(std::move(c));
        } else {
            auto& cur = result[it->second];
            const int sNew = score(c);
            const int sCur = score(cur);
            if (sNew > sCur || (sNew == sCur && (c.nickName.size()+c.name.size()) > (cur.nickName.size()+cur.name.size()))) {
                cur = std::move(c);
            }
        }
    }

    out.controls.swap(result);
}

/* ---------------------------------- API ---------------------------------- */

bool FanControlImport::LoadAndMap(const std::string& path,
                                  const std::vector<HwmonTemp>& temps,
                                  const std::vector<HwmonPwm>&  pwms,
                                  Profile& out,
                                  std::string& err,
                                  ProgressFn onProgress,
                                  json* detailsOut)
{
    auto progress = [&](int pct, const std::string& msg) {
        if (onProgress) onProgress(pct, msg);
        else LOG_DEBUG("import: state=running progress=%d msg=%s", pct, msg.c_str());
    };

    try {
        err.clear();
        if (detailsOut) *detailsOut = json::object();

        progress(0, "Reading FanControl profile...");
        json root = read_json_file(path);
        const json& main = (root.contains("Main") && root["Main"].is_object()) ? root["Main"] : root;

        size_t srcCurveCount = 0, addedControls = 0, skippedDup = 0, skippedNoCurve = 0;
        size_t pwmMapped = 0, pwmUnmapped = 0;

        out = Profile{};
        out.schema      = "lfc.profile/v1";
        out.name        = j2s(main, "ProfileName");
        out.description = {};
        out.lfcdVersion = LFCD_VERSION;
        out.fanCurves.clear();
        out.controls.clear();
        out.hwmons.clear();

        // collect involved hwmons
        for (const auto& t : temps) {
            if (std::none_of(out.hwmons.begin(), out.hwmons.end(),
                [&](const HwmonDeviceMeta& m){ return m.hwmonPath == t.chipPath; })) {
                HwmonDeviceMeta dm; dm.hwmonPath=t.chipPath; out.hwmons.push_back(std::move(dm));
            }
        }
        for (const auto& p : pwms) {
            if (std::none_of(out.hwmons.begin(), out.hwmons.end(),
                [&](const HwmonDeviceMeta& m){ return m.hwmonPath == p.chipPath; })) {
                HwmonDeviceMeta dm; dm.hwmonPath=p.chipPath; out.hwmons.push_back(std::move(dm));
            }
        }

        /* ------------------------------- curves ------------------------------- */
        progress(10, "Parsing curves...");
        if (main.contains("FanCurves") && main["FanCurves"].is_array()) {
            srcCurveCount = main["FanCurves"].size();
            std::unordered_set<std::string> curveNames;

            for (const auto& cj : main["FanCurves"]) {
                if (!cj.is_object()) continue;

                FanCurveMeta fc;
                fc.name = j2s(cj, "Name");
                if (fc.name.empty()) continue;

                const bool hasPoints = cj.contains("Points") && cj["Points"].is_array() && !cj["Points"].empty();
                const bool hasMix    = cj.contains("SelectedFanCurves") && cj["SelectedFanCurves"].is_array() && !cj["SelectedFanCurves"].empty();
                const bool hasIdle   = cj.contains("IdleTemperature") && cj.contains("IdleFanSpeed");
                const bool hasLoad   = cj.contains("LoadTemperature") && cj.contains("LoadFanSpeed");
                const bool nameTrig  = util::icontains(fc.name, "trigger");

                if (hasMix) {
                    fc.type = "mix";
                    for (const auto& v : cj["SelectedFanCurves"]) {
                        if (v.is_object() && v.contains("Name") && v["Name"].is_string())
                            fc.curveRefs.push_back(v["Name"].get<std::string>());
                        else if (v.is_string())
                            fc.curveRefs.push_back(v.get<std::string>());
                    }
                    int fnIdx = (int)j2d(cj, "SelectedMixFunction", 1.0);
                    switch (fnIdx) {
                        case 0: fc.mix = MixFunction::Min; break;
                        case 2: fc.mix = MixFunction::Max; break;
                        default: fc.mix = MixFunction::Avg; break;
                    }
                } else if ((hasIdle && hasLoad) || nameTrig) {
                    fc.type = "trigger";
                    fc.onC  = j2d(cj, "LoadTemperature", j2d(cj, "TriggerOn",  0.0));
                    fc.offC = j2d(cj, "IdleTemperature", j2d(cj, "TriggerOff", 0.0));
                    collectCurveTempSources(cj, fc.tempSensors, temps);
                } else if (hasPoints) {
                    fc.type = "graph";
                    collectCurveTempSources(cj, fc.tempSensors, temps);
                    auto pts = parse_points(cj["Points"]);
                    fc.points.insert(fc.points.end(), pts.begin(), pts.end());
                } else if (hasIdle && hasLoad) {
                    fc.type = "graph";
                    const double idleT = j2d(cj, "IdleTemperature", 0.0);
                    const double idleP = j2d(cj, "IdleFanSpeed",    0.0);
                    const double loadT = j2d(cj, "LoadTemperature", 0.0);
                    const double loadP = j2d(cj, "LoadFanSpeed",    0.0);
                    if (idleT <= loadT) { fc.points.push_back({idleT, idleP}); fc.points.push_back({loadT, loadP}); }
                    else                { fc.points.push_back({loadT, loadP}); fc.points.push_back({idleT, idleP}); }
                    collectCurveTempSources(cj, fc.tempSensors, temps);
                } else {
                    // unknown → skip
                }

                if (fc.name.empty()) continue;
                if (!curveNames.insert(fc.name).second) { ++skippedDup; continue; }

                LOG_DEBUG("import: curve '%s' type=%s sensors=%zu refs=%zu points=%zu on=%.2f off=%.2f mix=%d",
                          fc.name.c_str(), fc.type.c_str(),
                          fc.tempSensors.size(), fc.curveRefs.size(), fc.points.size(),
                          fc.onC, fc.offC, (int)fc.mix);

                out.fanCurves.push_back(std::move(fc));
            }
        }

        // NEW: make mix curves advertise union of referenced sensors (recursive)
        populate_mix_sensor_unions(out);

        /* ------------------------------ controls ------------------------------ */
        progress(65, "Parsing controls...");
        if (main.contains("Controls") && main["Controls"].is_array()) {
            for (const auto& cj : main["Controls"]) {
                if (!cj.is_object()) continue;

                ControlMeta cm;
                cm.name        = j2s(cj, "Name");
                cm.nickName    = j2s(cj, "NickName");
                cm.enabled     = j2b(cj, "Enable", true);
                cm.hidden      = j2b(cj, "IsHidden", false);
                cm.pwmPath     = j2s(cj, "Identifier");

                if (cj.contains("SelectedFanCurve")) {
                    const auto& sfc = cj["SelectedFanCurve"];
                    if (sfc.is_string()) cm.curveRef = sfc.get<std::string>();
                    else if (sfc.is_object() && sfc.contains("Name") && sfc["Name"].is_string())
                        cm.curveRef = sfc["Name"].get<std::string>();
                }

                cm.manual        = j2b(cj, "ManualControl", false);
                cm.manualPercent = (int)std::round(j2d(cj, "ManualControlValue", (double)cm.manualPercent));

                if (!cm.enabled && cm.curveRef.empty() && !cm.manual) { ++skippedNoCurve; continue; }

                LOG_DEBUG("import: control '%s' nick='%s' enabled=%d manual=%d curveRef='%s' ident='%s'",
                          cm.name.c_str(), cm.nickName.c_str(),
                          (int)cm.enabled, (int)cm.manual,
                          cm.curveRef.c_str(), cm.pwmPath.c_str());

                out.controls.push_back(std::move(cm));
                ++addedControls;
            }
        }

        /* -------- disable controls whose curve has no effective sensors ------- */
        for (auto& c : out.controls) {
            if (c.manual || !c.enabled) continue;
            if (c.curveRef.empty()) {
                LOG_DEBUG("import: disabling control '%s' (no curve assigned → no sensors)", (c.nickName.empty()?c.name:c.nickName).c_str());
                c.enabled = false;
                continue;
            }
            const FanCurveMeta* fc = nullptr;
            for (const auto& f : out.fanCurves) if (f.name == c.curveRef) { fc = &f; break; }
            if (!fc || !curveHasEffectiveSensors(out, *fc)) {
                LOG_DEBUG("import: disabling control '%s' (curve '%s' has no effective sensors)",
                          (c.nickName.empty()?c.name:c.nickName).c_str(), c.curveRef.c_str());
                c.enabled = false;
            }
        }

        /* ------------------------- PWM mapping + dedupe ------------------------ */
        progress(80, "Mapping PWMs to controls...");
        size_t enabledBefore = 0;
        for (const auto& c : out.controls) if (c.enabled) ++enabledBefore;

        size_t mapped = 0, unmapped = 0;
        for (auto& c : out.controls) {
            if (!c.enabled && !c.manual) { ++unmapped; continue; }

            std::string mappedPwm;
            if (!c.curveRef.empty()) mappedPwm = pickPwmForCurveOnSameChip(out, pwms, c.curveRef);
            if (mappedPwm.empty() && !c.pwmPath.empty() && c.pwmPath.rfind("/sys/", 0) != 0)
                mappedPwm = map_windows_control_identifier_to_pwm(c.pwmPath, pwms);

            if (!mappedPwm.empty()) { c.pwmPath = mappedPwm; ++mapped; }
            else {
                if (!c.enabled && c.pwmPath.rfind("/sys/", 0) != 0) c.pwmPath.clear();
                ++unmapped;
            }
        }
        dedupe_controls_by_pwm(out);

        pwmMapped = mapped; pwmUnmapped = unmapped;
        LOG_DEBUG("import: pwm mapping result mapped=%zu unmapped=%zu (post-dedupe controls=%zu)",
                  pwmMapped, pwmUnmapped, out.controls.size());

        /* -------------------------------- summary -------------------------------- */
        progress(85, "Summarizing...");
        if (detailsOut) {
            (*detailsOut)["source"]                  = "FanControl.Release";
            (*detailsOut)["profileName"]             = out.name;
            (*detailsOut)["curves"]["sourceCount"]   = (int)srcCurveCount;
            (*detailsOut)["curves"]["kept"]          = (int)out.fanCurves.size();
            (*detailsOut)["curves"]["dupesSkipped"]  = (int)skippedDup;
            (*detailsOut)["controls"]["added"]       = (int)addedControls;
            (*detailsOut)["controls"]["enabledIn"]   = (int)enabledBefore;
            (*detailsOut)["controls"]["enabledOut"]  = (int)std::count_if(out.controls.begin(), out.controls.end(),
                                                          [](const ControlMeta& c){ return c.enabled; });
            (*detailsOut)["pwm"]["mapped"]           = (int)pwmMapped;
            (*detailsOut)["pwm"]["unmapped"]         = (int)pwmUnmapped;
        }

        progress(99, "done");
        return true;

    } catch (const std::exception& e) {
        err = e.what();
        return false;
    } catch (...) {
        err = "unknown error";
        return false;
    }
}

} // namespace lfc
