/*
 * Linux Fan Control — FanControl.Release importer (implementation)
 * (c) 2025 LinuxFanControl contributors
 *
 * Maps FanControl.Release userConfig.json into LFC Profile schema:
 * - Graph curves  -> points + temp sensor
 * - Trigger curves-> converted to 2-point graph (Idle/Load) + onC/offC stored
 * - Mix curves    -> type="mix" with curveRefs + mix function (min/avg/max)
 * - Controls      -> curveRef + pwm mapping; NickName preserved
 * - Reverse map   -> FanCurveMeta.controlRefs filled from Controls[].SelectedFanCurve
 */
#include "include/FanControlImport.hpp"
#include "include/Version.hpp"
#include "include/Log.hpp"
#include "include/Hwmon.hpp"
#include "include/VendorMapping.hpp"
#include "include/Profile.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <optional>
#include <set>
#include <sstream>
#include <unordered_map>

using nlohmann::json;

namespace {

static std::string trim(std::string s) {
    auto notsp = [](unsigned char c){ return !std::isspace(c); };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), notsp));
    s.erase(std::find_if(s.rbegin(), s.rend(), notsp).base(), s.end());
    return s;
}

static bool parsePair(const std::string& item, double& tc, int& pc) {
    auto comma = item.find(',');
    if (comma == std::string::npos) return false;
    const std::string t = trim(item.substr(0, comma));
    const std::string p = trim(item.substr(comma + 1));
    try {
        tc = std::stod(t);
        pc = static_cast<int>(std::stod(p));
        return true;
    } catch (...) {
        return false;
    }
}

static std::vector<lfc::CurvePoint> parsePoints(const json& any) {
    std::vector<lfc::CurvePoint> out;

    // FanControl.Release stores either a single string "t,p; t,p; ..." or an array ["t,p","t,p",...]
    if (any.is_string()) {
        std::stringstream ss(any.get<std::string>());
        std::string item;
        while (std::getline(ss, item, ';')) {
            item = trim(item);
            if (item.empty()) continue;
            double tc; int pc;
            if (parsePair(item, tc, pc)) out.push_back(lfc::CurvePoint{tc, pc});
        }
    } else if (any.is_array()) {
        for (const auto& s : any) {
            if (!s.is_string()) continue;
            const std::string item = trim(s.get<std::string>());
            if (item.empty()) continue;
            double tc; int pc;
            if (parsePair(item, tc, pc)) out.push_back(lfc::CurvePoint{tc, pc});
        }
    }

    return out;
}

// Extract chip token & index from FanControl.Identifier like "/lpc/nct6799d/temperature/6" or "/lpc/nct6799d/control/0"
struct IdParts { std::string chip; int index{-1}; std::string kind; };
static std::optional<IdParts> parseIdentifier(const std::string& id) {
    std::vector<std::string> tok;
    std::stringstream ss(id);
    std::string part;
    while (std::getline(ss, part, '/')) if (!part.empty()) tok.push_back(part);
    if (tok.size() < 4) return std::nullopt;
    int idx = -1;
    try { idx = std::stoi(tok[tok.size()-1]); } catch (...) { idx = -1; }
    return IdParts{tok[tok.size()-3], idx, tok[tok.size()-2]};
}

static const lfc::HwmonTemp* findBestTempByIdentifier(const std::vector<lfc::HwmonTemp>& temps,
                                                      const std::string& identifier)
{
    auto parsed = parseIdentifier(identifier);
    if (!parsed) return nullptr;

    const auto& chipTok = parsed->chip;
    const int   idx     = parsed->index;

    std::vector<std::string> aliases;
    aliases.push_back(chipTok);
    try {
        auto extra = lfc::VendorMapping::instance().chipAliasesFor(chipTok);
        aliases.insert(aliases.end(), extra.begin(), extra.end());
    } catch (...) {}

    const lfc::HwmonTemp* best = nullptr;

    // 1) exact chip alias match + path suffix contains "temp{idx}_input"
    if (idx > 0) {
        const std::string suffix = std::string("/temp") + std::to_string(idx) + "_input";
        for (const auto& t : temps) {
            for (const auto& a : aliases) {
                if (t.chipPath.find(a) != std::string::npos &&
                    t.path_input.size() >= suffix.size() &&
                    t.path_input.rfind(suffix) == t.path_input.size() - suffix.size())
                {
                    return &t;
                }
            }
        }
    }

    // 2) chip alias match only
    for (const auto& t : temps) {
        for (const auto& a : aliases) {
            if (t.chipPath.find(a) != std::string::npos) {
                best = &t;
                break;
            }
        }
        if (best) break;
    }

    // 3) fallback: same index on any chip
    if (!best && idx > 0) {
        const std::string suffix = std::string("/temp") + std::to_string(idx) + "_input";
        for (const auto& t : temps) {
            if (t.path_input.size() >= suffix.size() &&
                t.path_input.rfind(suffix) == t.path_input.size() - suffix.size())
            {
                best = &t; break;
            }
        }
    }

    return best;
}

static lfc::MixFunction mixFnFromInt(int v) {
    switch (v) {
        case 0: return lfc::MixFunction::Min;
        case 2: return lfc::MixFunction::Max;
        default: return lfc::MixFunction::Avg;
    }
}

} // namespace

namespace lfc {

bool FanControlImport::LoadAndMap(const std::string& path,
                                  const std::vector<HwmonTemp>& temps,
                                  const std::vector<HwmonPwm>& pwms,
                                  Profile& out,
                                  std::string& err,
                                  ProgressFn onProgress,
                                  nlohmann::json* detailsOut)
{
    try {
        onProgress(0, "read");
        std::ifstream f(path);
        if (!f) {
            err = "cannot open config file";
            return false;
        }
        json j = json::parse(f, nullptr, true, true);

        if (!j.contains("Main") || !j.at("Main").is_object()) {
            err = "invalid FanControl.Release config: missing Main";
            return false;
        }
        const json& main = j.at("Main");

        out.schema      = "lfc.profile/v1";
        out.name        = j.value("Name", std::string{"Imported"});
        out.description = "Imported from FanControl.Release";
        out.lfcdVersion = LFCD_VERSION;

        // inventory
        onProgress(5, "inventory");
        {
            std::set<std::string> chips;
            for (const auto& p : pwms) chips.insert(p.chipPath);
            for (const auto& ch : chips) {
                HwmonDeviceMeta d;
                d.hwmonPath = ch;
                d.name      = ch;
                d.vendor    = VendorMapping::instance().vendorForChipName(ch);
                out.hwmons.push_back(std::move(d));
            }
        }

        // curves
        onProgress(20, "curves");
        std::unordered_map<std::string, FanCurveMeta> curvesByName;

        const json& mainTemps = main.value("TemperatureSensors", json::array());
        if (main.contains("FanCurves") && main.at("FanCurves").is_array()) {
            for (const auto& fc : main.at("FanCurves")) {
                if (!fc.contains("Name") || !fc.at("Name").is_string()) continue;
                const std::string name = fc.at("Name").get<std::string>();

                FanCurveMeta meta;
                meta.name = name;

                // Mix curve
                if (fc.contains("SelectedFanCurves") && fc.at("SelectedFanCurves").is_array()) {
                    meta.type = "mix";
                    meta.mix  = mixFnFromInt(fc.value("SelectedMixFunction", 1));
                    for (const auto& ref : fc.at("SelectedFanCurves")) {
                        if (ref.contains("Name") && ref.at("Name").is_string()) {
                            meta.curveRefs.push_back(ref.at("Name").get<std::string>());
                        }
                    }
                }
                // Trigger-like (Idle/Load)
                else if (fc.contains("IdleTemperature") || fc.contains("LoadTemperature")) {
                    meta.type = "graph";
                    const double idleT = fc.value("IdleTemperature", 0.0);
                    const int    idleP = fc.value("IdleFanSpeed",    0);
                    const double loadT = fc.value("LoadTemperature", 0.0);
                    const int    loadP = fc.value("LoadFanSpeed",    0);
                    if (idleT > 0 || loadT > 0) {
                        meta.points.push_back(CurvePoint{idleT, idleP});
                        meta.points.push_back(CurvePoint{loadT, loadP});
                    }
                    meta.onC  = loadT;
                    meta.offC = idleT;

                    // SelectedTempSource by Identifier
                    if (fc.contains("SelectedTempSource") && fc.at("SelectedTempSource").is_object()) {
                        const json& sts = fc.at("SelectedTempSource");
                        if (sts.contains("Identifier") && sts.at("Identifier").is_string()) {
                            const std::string ident = sts.at("Identifier").get<std::string>();
                            if (auto t = findBestTempByIdentifier(temps, ident)) {
                                meta.tempSensors.push_back(t->path_input);
                            }
                        }
                    }
                }
                // Graph curve with Points
                else if (fc.contains("Points")) {
                    meta.type   = "graph";
                    meta.points = parsePoints(fc.at("Points"));

                    // SelectedTempSource by Identifier
                    if (fc.contains("SelectedTempSource") && fc.at("SelectedTempSource").is_object()) {
                        const json& sts = fc.at("SelectedTempSource");
                        if (sts.contains("Identifier") && sts.at("Identifier").is_string()) {
                            const std::string ident = sts.at("Identifier").get<std::string>();
                            if (auto t = findBestTempByIdentifier(temps, ident)) {
                                meta.tempSensors.push_back(t->path_input);
                            }
                        }
                    }
                } else {
                    meta.type = "graph";
                }

                curvesByName.emplace(meta.name, std::move(meta));
            }
        }

        // preserve order from source
        if (main.contains("FanCurves") && main.at("FanCurves").is_array()) {
            for (const auto& fc : main.at("FanCurves")) {
                const std::string nm = fc.value("Name", std::string{});
                auto it = curvesByName.find(nm);
                if (it != curvesByName.end()) out.fanCurves.push_back(std::move(it->second));
            }
        }

        // controls
        onProgress(60, "controls");
        if (main.contains("Controls") && main.at("Controls").is_array()) {
            for (const auto& c : main.at("Controls")) {
                ControlMeta cm;
                cm.name     = c.value("Name", std::string{});
                cm.nickName = c.value("NickName", std::string{});

                if (c.contains("SelectedFanCurve") && c.at("SelectedFanCurve").is_object()) {
                    cm.curveRef = c.at("SelectedFanCurve").value("Name", std::string{});
                }

                if (c.contains("Identifier") && c.at("Identifier").is_string()) {
                    const std::string id = c.at("Identifier").get<std::string>();
                    auto parts = parseIdentifier(id);
                    if (parts) {
                        const int want = (parts->index >= 0) ? (parts->index + 1) : -1;
                        for (const auto& p : pwms) {
                            bool chipOk = p.chipPath.find(parts->chip) != std::string::npos;
                            bool idxOk  = (want > 0) && p.path_pwm.size() >= 4 &&
                                          p.path_pwm.rfind(std::string("/pwm") + std::to_string(want)) != std::string::npos;
                            if (chipOk && (want <= 0 || idxOk)) {
                                cm.pwmPath = p.path_pwm;
                                break;
                            }
                        }
                        if (cm.pwmPath.empty() && !pwms.empty()) cm.pwmPath = pwms.front().path_pwm;
                    }
                }

                // reverse mapping: add this control as a user of the referenced curve
                if (!cm.curveRef.empty()) {
                    for (auto& fcMeta : out.fanCurves) {
                        if (fcMeta.name == cm.curveRef) {
                            fcMeta.controlRefs.push_back(cm.name);
                            break;
                        }
                    }
                }

                out.controls.push_back(std::move(cm));
            }
        }

        if (detailsOut) {
            json det;
            det["source"] = path;
            det["curves"] = out.fanCurves.size();
            det["controls"] = out.controls.size();
            *detailsOut = det;
        }

        onProgress(100, "ok");
        return true;
    } catch (const std::exception& ex) {
        err = ex.what();
        return false;
    } catch (...) {
        err = "unknown error";
        return false;
    }
}

} // namespace lfc
