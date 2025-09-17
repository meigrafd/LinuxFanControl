/*
 * Linux Fan Control — RPC handlers (implementation)
 * (c) 2025 LinuxFanControl contributors
 */

#include "RpcHandlers.hpp"

#include "Daemon.hpp"
#include "Config.hpp"
#include "Hwmon.hpp"
#include "Engine.hpp"
#include "Profile.hpp"
#include "Detection.hpp"
#include "FanControlImport.hpp"
#include "UpdateChecker.hpp"
#include "Version.hpp"
#include "include/CommandRegistry.h"

#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <set>
#include <limits>
#include <chrono>
#include <thread>
#include <cctype>

using nlohmann::json;

namespace lfc {

// ----- utils -----

static inline std::string jstr(const std::string& s) {
    std::string out; out.reserve(s.size()+2);
    out.push_back('"');
    for (char c : s) {
        if (c=='\\' || c=='"') { out.push_back('\\'); out.push_back(c); }
        else if (c=='\n') out += "\\n";
        else out.push_back(c);
    }
    out.push_back('"');
    return out;
}
static inline RpcResult ok(const char* method, const std::string& dataJson) {
    std::ostringstream ss;
    ss << "{\"method\":" << jstr(method) << ",\"success\":true";
    if (!dataJson.empty()) ss << ",\"data\":" << dataJson;
    ss << "}";
    return {true, ss.str()};
}
static inline RpcResult err(const char* method, int code, const std::string& msg) {
    std::ostringstream ss;
    ss << "{\"method\":" << jstr(method) << ",\"success\":false,"
       << "\"error\":{\"code\":" << code << ",\"message\":" << jstr(msg) << "}}";
    return {false, ss.str()};
}
static int clampi(int v, int lo, int hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}
static double clampd(double v, double lo, double hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static std::string to_profile_name(std::string s) {
    if (s.size() >= 5) {
        std::string tail = s.substr(s.size()-5);
        for (char& c : tail) c = (char)std::tolower((unsigned char)c);
        if (tail == ".json") s.erase(s.size()-5);
    }
    return s;
}
static std::filesystem::path file_for_profile(const std::string& dir, const std::string& name) {
    return std::filesystem::path(dir) / (name + std::string(".json"));
}

static json inv_warnings(const HwmonInventory& inv) {
    json arr = json::array();
    if (inv.temps.empty()) arr.push_back("No temperature sensors found.");
    if (inv.fans.empty())  arr.push_back("No tach inputs found.");
    if (inv.pwms.empty())  arr.push_back("No PWM outputs found.");
    return arr;
}

static int index_of_pwm(const HwmonInventory& inv, const std::string& pwmPath) {
    for (size_t i=0;i<inv.pwms.size();++i)
        if (inv.pwms[i].path_pwm == pwmPath) return (int)i;
    return -1;
}
static bool has_temp(const HwmonInventory& inv, const std::string& tempPath) {
    for (const auto& t : inv.temps) if (t.path_input == tempPath) return true;
    return false;
}

// Start detection if needed; then wait (no timeout) until finished by polling status.
static bool run_detection_sync(Daemon& self, std::vector<int>& outRpm) {
    outRpm.clear();
    auto s0 = self.detectionStatus();
    if (!s0.running) (void)self.detectionStart();

    while (true) {
        auto st = self.detectionStatus();
        if (!st.running && st.total > 0) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    outRpm = self.detectionResults();
    return true;
}

// Shared verification for verifyMapping/importAs.
static json build_validation_report(Daemon& self,
                                    const Profile& prof,
                                    const HwmonInventory& inv,
                                    bool withDetect,
                                    int rpmMin)
{
    json report;
    json errs = json::array();
    json warns = json::array();

    json pwmItems = json::array();
    json tempItems = json::array();
    std::vector<int> usedPwmIdx; usedPwmIdx.reserve(prof.rules.size());

    for (const auto& rule : prof.rules) {
        int pidx = index_of_pwm(inv, rule.pwmPath);
        pwmItems.push_back({{"pwmPath", rule.pwmPath}, {"exists", pidx >= 0}});
        if (pidx < 0) errs.push_back(std::string("Missing PWM path: ") + rule.pwmPath);
        usedPwmIdx.push_back(pidx);

        if (rule.sources.empty())
            errs.push_back(std::string("Rule has no sources for PWM: ") + rule.pwmPath);

        for (const auto& sc : rule.sources) {
            if (sc.tempPaths.empty())
                warns.push_back(std::string("Source has no temperature paths for PWM: ") + rule.pwmPath);
            for (const auto& tp : sc.tempPaths) {
                bool ok = has_temp(inv, tp);
                tempItems.push_back({{"tempPath", tp}, {"exists", ok}});
                if (!ok) errs.push_back(std::string("Missing temp path: ") + tp);
            }
            double lastT = -1e9; int bad = 0;
            for (const auto& pt : sc.points) {
                if (pt.tempC < lastT - 1e-9) ++bad;
                if (pt.percent < 0 || pt.percent > 100) ++bad;
                lastT = pt.tempC;
            }
            if (sc.settings.minPercent < 0 || sc.settings.minPercent > 100 ||
                sc.settings.maxPercent < 0 || sc.settings.maxPercent > 100 ||
                sc.settings.minPercent > sc.settings.maxPercent) ++bad;
            if (bad > 0) errs.push_back(std::string("Invalid curve/settings for PWM: ") + rule.pwmPath);
        }
    }

    report["pwms"]  = pwmItems;
    report["temps"] = tempItems;

    json detect;
    detect["requested"] = withDetect;
    detect["rpmMin"]    = rpmMin;

    if (withDetect) {
        std::vector<int> res;
        bool ran = run_detection_sync(self, res);
        detect["ran"] = ran;
        if (!ran) {
            errs.push_back("Detection timed out or failed.");
        } else {
            detect["results"] = json::array();
            for (int v : res) detect["results"].push_back(v);
            for (int idx : usedPwmIdx) {
                if (idx < 0 || idx >= (int)res.size()) continue;
                if (res[idx] < rpmMin) errs.push_back("Detection found no PWM reaching rpmMin.");
            }
        }
    } else {
        detect["ran"] = false;
    }
    report["detect"] = detect;

    report["errors"]   = errs;
    report["warnings"] = warns;
    report["ok"]       = errs.empty();
    return report;
}

// ----- bindings -----

void BindDaemonRpcCommands(Daemon& self, CommandRegistry& reg) {
    // meta
    reg.registerMethod("ping", "Health check", [&](const RpcRequest&) -> RpcResult {
        return ok("ping", "{}");
    });
    reg.registerMethod("version", "RPC and daemon version", [&](const RpcRequest&) -> RpcResult {
        json j; j["daemon"]="lfcd"; j["version"]=LFCD_VERSION; j["rpc"]=1;
        return ok("version", j.dump());
    });
    reg.registerMethod("rpc.commands", "List available RPC methods", [&](const RpcRequest&) -> RpcResult {
        auto list = self.listRpcCommands();
        json arr = json::array();
        for (const auto& c : list) arr.push_back({{"name", c.name}, {"help", c.help}});
        return ok("rpc.commands", arr.dump());
    });

    // config
    reg.registerMethod("config.load", "Load daemon config from disk", [&](const RpcRequest&) -> RpcResult {
        std::string e;
        DaemonConfig tmp = loadDaemonConfig(self.configPath(), &e);
        if (!e.empty()) return err("config.load", -32002, "config load failed");

        json out;
        out["log"]["file"]           = tmp.logfile;
        out["log"]["debug"]          = tmp.debug;
        out["rpc"]["host"]           = tmp.host;
        out["rpc"]["port"]           = tmp.port;
        out["shm"]["path"]           = tmp.shmPath;
        out["profiles"]["dir"]       = tmp.profilesDir;
        out["profiles"]["active"]    = tmp.profileName;
        out["pidFile"]               = tmp.pidfile;
        out["engine"]["deltaC"]      = self.engineDeltaC();
        out["engine"]["forceTickMs"] = self.engineForceTickMs();
        out["engine"]["tickMs"]      = self.engineTickMs();
        return ok("config.load", out.dump());
    });

    reg.registerMethod("config.save", "Save daemon config to disk; params:{config:object}", [&](const RpcRequest& rq) -> RpcResult {
        if (rq.params.empty()) return err("config.save", -32602, "bad params");
        json p = json::parse(rq.params, nullptr, false);
        if (p.is_discarded() || !p.contains("config") || !p["config"].is_object())
            return err("config.save", -32602, "missing config");

        DaemonConfig nc = self.cfg();
        auto& jc = p["config"];
        if (jc.contains("log")) {
            if (jc["log"].contains("file") && jc["log"]["file"].is_string()) nc.logfile = jc["log"]["file"].get<std::string>();
            if (jc["log"].contains("debug") && jc["log"]["debug"].is_boolean()) nc.debug = jc["log"]["debug"].get<bool>();
        }
        if (jc.contains("rpc")) {
            if (jc["rpc"].contains("host") && jc["rpc"]["host"].is_string()) nc.host = jc["rpc"]["host"].get<std::string>();
            if (jc["rpc"].contains("port") && jc["rpc"]["port"].is_number_integer()) nc.port = jc["rpc"]["port"].get<int>();
        }
        if (jc.contains("shm")) {
            if (jc["shm"].contains("path") && jc["shm"]["path"].is_string()) nc.shmPath = jc["shm"]["path"].get<std::string>();
        }
        if (jc.contains("profiles")) {
            if (jc["profiles"].contains("dir") && jc["profiles"]["dir"].is_string()) nc.profilesDir = jc["profiles"]["dir"].get<std::string>();
            if (jc["profiles"].contains("active") && jc["profiles"]["active"].is_string())
                nc.profileName = to_profile_name(jc["profiles"]["active"].get<std::string>());
        }
        if (jc.contains("pidFile") && jc["pidFile"].is_string()) nc.pidfile = jc["pidFile"].get<std::string>();
        if (jc.contains("engine") && jc["engine"].is_object()) {
            if (jc["engine"].contains("deltaC") && jc["engine"]["deltaC"].is_number())
                self.setEngineDeltaC(clampd(jc["engine"]["deltaC"].get<double>(), 0.0, 10.0));
            if (jc["engine"].contains("forceTickMs") && jc["engine"]["forceTickMs"].is_number_integer())
                self.setEngineForceTickMs(clampi(jc["engine"]["forceTickMs"].get<int>(), 100, 10000));
            if (jc["engine"].contains("tickMs") && jc["engine"]["tickMs"].is_number_integer())
                self.setEngineTickMs(clampi(jc["engine"]["tickMs"].get<int>(), 5, 1000));
        }

        std::string e;
        if (!saveDaemonConfig(nc, self.configPath(), &e)) return err("config.save", -32003, e);
        self.setCfg(nc);
        return ok("config.save", "{}");
    });

    reg.registerMethod("config.set", "Set single key; params:{key:string, value:any}", [&](const RpcRequest& rq) -> RpcResult {
        if (rq.params.empty()) return err("config.set", -32602, "bad params");
        json p = json::parse(rq.params, nullptr, false);
        if (p.is_discarded() || !p.contains("key")) return err("config.set", -32602, "missing key");
        std::string key = p["key"].get<std::string>();
        json val = p.contains("value") ? p["value"] : json();

        DaemonConfig nc = self.cfg();
        if (key == "log.debug") {
            if (val.is_boolean()) nc.debug = val.get<bool>();
        } else if (key == "log.file") {
            if (val.is_string()) nc.logfile = val.get<std::string>();
        } else if (key == "rpc.host") {
            if (val.is_string()) nc.host = val.get<std::string>();
        } else if (key == "rpc.port") {
            if (val.is_number_integer()) nc.port = val.get<int>();
        } else if (key == "shm.path") {
            if (val.is_string()) nc.shmPath = val.get<std::string>();
        } else if (key == "profiles.dir") {
            if (val.is_string()) nc.profilesDir = val.get<std::string>();
        } else if (key == "profiles.active") {
            if (val.is_string()) nc.profileName = to_profile_name(val.get<std::string>());
        } else if (key == "pidFile") {
            if (val.is_string()) nc.pidfile = val.get<std::string>();
        } else if (key == "engine.deltaC") {
            double v = self.engineDeltaC();
            if (val.is_number()) v = val.get<double>();
            else if (val.is_string()) { try { v = std::stod(val.get<std::string>()); } catch (...) {} }
            self.setEngineDeltaC(clampd(v, 0.0, 10.0));
        } else if (key == "engine.forceTickMs") {
            int v = self.engineForceTickMs();
            if (val.is_number_integer()) v = val.get<int>();
            else if (val.is_string()) { try { v = std::stoi(val.get<std::string>()); } catch (...) {} }
            self.setEngineForceTickMs(clampi(v, 100, 10000));
        } else if (key == "engine.tickMs") {
            int v = self.engineTickMs();
            if (val.is_number_integer()) v = val.get<int>();
            else if (val.is_string()) { try { v = std::stoi(val.get<std::string>()); } catch (...) {} }
            self.setEngineTickMs(clampi(v, 5, 1000));
        } else {
            return err("config.set", -32602, "unknown key");
        }

        std::string e;
        if (!saveDaemonConfig(nc, self.configPath(), &e)) return err("config.set", -32003, e);
        self.setCfg(nc);
        return ok("config.set", "{}");
    });

    // hwmon / list
    reg.registerMethod("hwmon.snapshot", "Counts of discovered devices", [&](const RpcRequest&) -> RpcResult {
        const auto& inv = self.hwmon();
        json j; j["temps"]=inv.temps.size(); j["fans"]=inv.fans.size(); j["pwms"]=inv.pwms.size();
        return ok("hwmon.snapshot", j.dump());
    });
    reg.registerMethod("list.sensor", "List temperature sensors with current values", [&](const RpcRequest&) -> RpcResult {
        const auto& inv = self.hwmon();
        json arr = json::array();
        for (size_t i=0;i<inv.temps.size();++i) {
            auto t = Hwmon::readTempC(inv.temps[i]).value_or(std::numeric_limits<double>::quiet_NaN());
            arr.push_back({{"index",(int)i},{"path",inv.temps[i].path_input},{"label",inv.temps[i].label},{"tempC",t}});
        }
        return ok("list.sensor", arr.dump());
    });
    reg.registerMethod("list.fan", "List fan tach inputs", [&](const RpcRequest&) -> RpcResult {
        const auto& inv = self.hwmon();
        json arr = json::array();
        for (size_t i=0;i<inv.fans.size();++i) {
            int rpm = Hwmon::readRpm(inv.fans[i]).value_or(0);
            arr.push_back({{"index",(int)i},{"path",inv.fans[i].path_input},{"rpm",rpm}});
        }
        return ok("list.fan", arr.dump());
    });
    reg.registerMethod("list.pwm", "List PWM outputs", [&](const RpcRequest&) -> RpcResult {
        const auto& inv = self.hwmon();
        json arr = json::array();
        for (size_t i=0;i<inv.pwms.size();++i) {
            int en  = Hwmon::readEnable(inv.pwms[i]).value_or(-1);
            int pct = Hwmon::readPercent(inv.pwms[i]).value_or(-1);
            int raw = Hwmon::readRaw(inv.pwms[i]).value_or(-1);
            arr.push_back({{"index",(int)i},{"pwm",inv.pwms[i].path_pwm},{"enable",en},{"percent",pct},{"raw",raw},{"enablePath",inv.pwms[i].path_enable}});
        }
        return ok("list.pwm", arr.dump());
    });
    reg.registerMethod("list.profiles", "List profiles (names only) in profiles dir", [&](const RpcRequest&) -> RpcResult {
        json arr = json::array();
        const std::string dir = self.cfg().profilesDir;
        if (!dir.empty() && std::filesystem::exists(dir)) {
            for (auto& e : std::filesystem::directory_iterator(dir)) {
                if (!e.is_regular_file()) continue;
                auto p = e.path().filename().string();
                if (p.size() >= 5 && p.substr(p.size()-5) == ".json") p.erase(p.size()-5);
                arr.push_back(p);
            }
        }
        return ok("list.profiles", arr.dump());
    });

    // engine
    reg.registerMethod("engine.enable", "Enable automatic control", [&](const RpcRequest&) -> RpcResult {
        self.engineEnable(true);
        return ok("engine.enable", "{}");
    });
    reg.registerMethod("engine.disable", "Disable automatic control", [&](const RpcRequest&) -> RpcResult {
        self.engineEnable(false);
        return ok("engine.disable", "{}");
    });
    reg.registerMethod("engine.reset", "Disable control and clear profile", [&](const RpcRequest&) -> RpcResult {
        self.engineEnable(false);
        Profile empty;
        (void)self.applyProfile(empty);
        DaemonConfig nc = self.cfg();
        nc.profileName.clear();
        std::string serr;
        (void)saveDaemonConfig(nc, self.configPath(), &serr);
        self.setCfg(nc);
        return ok("engine.reset", "{}");
    });
    reg.registerMethod("engine.status", "Basic engine status", [&](const RpcRequest&) -> RpcResult {
        json j;
        j["enabled"]      = self.engineControlEnabled();
        j["deltaC"]       = self.engineDeltaC();
        j["tickMs"]       = self.engineTickMs();
        j["forceTickMs"]  = self.engineForceTickMs();
        j["activeProfile"]= self.cfg().profileName;
        return ok("engine.status", j.dump());
    });

    // detection
    reg.registerMethod("detect.start", "Start non-blocking detection", [&](const RpcRequest&) -> RpcResult {
        bool started = self.detectionStart();
        json j; j["started"] = started;
        return ok("detect.start", j.dump());
    });
    reg.registerMethod("detect.abort", "Abort detection", [&](const RpcRequest&) -> RpcResult {
        self.detectionAbort();
        return ok("detect.abort", "{}");
    });
    reg.registerMethod("detect.status", "Detection status/progress", [&](const RpcRequest&) -> RpcResult {
        auto s = self.detectionStatus();
        json j;
        j["running"] = s.running;
        j["currentIndex"] = s.currentIndex;
        j["total"] = s.total;
        j["phase"] = s.phase;
        return ok("detect.status", j.dump());
    });
    reg.registerMethod("detect.results", "Return detection peak RPMs per PWM", [&](const RpcRequest&) -> RpcResult {
        auto v = self.detectionResults();
        json arr = json::array();
        for (size_t i=0;i<v.size();++i) arr.push_back({{"index",(int)i},{"rpm",v[i]}});
        return ok("detect.results", arr.dump());
    });

    // profiles
    reg.registerMethod("profile.getActive", "Get active profile NAME (no extension)", [&](const RpcRequest&) -> RpcResult {
        json j; j["name"] = self.cfg().profileName;
        return ok("profile.getActive", j.dump());
    });

    reg.registerMethod("profile.verifyMapping",
        "Validate a stored profile or a path; params:{name?:string,path?:string,withDetect?:bool,rpmMin?:int}",
        [&](const RpcRequest& rq) -> RpcResult {
            if (rq.params.empty()) return err("profile.verifyMapping", -32602, "bad params");
            json p = json::parse(rq.params, nullptr, false);
            if (p.is_discarded()) return err("profile.verifyMapping", -32602, "bad params");

            std::string name, path;
            if (p.contains("name") && p["name"].is_string()) name = to_profile_name(p["name"].get<std::string>());
            if (p.contains("path") && p["path"].is_string()) path = p["path"].get<std::string>();
            bool withDetect = p.value("withDetect", false);
            int rpmMin = clampi(p.value("rpmMin", 300), 0, 10000);

            if (name.empty() && path.empty())
                return err("profile.verifyMapping", -32602, "missing name or path");

            Profile prof;
            std::string serr;
            if (!name.empty()) {
                auto full = file_for_profile(self.cfg().profilesDir, name);
                if (!std::filesystem::exists(full)) return err("profile.verifyMapping", -32004, "profile not found");
                if (!prof.loadFromFile(full.string(), &serr)) return err("profile.verifyMapping", -32005, serr);
            } else {
                if (!prof.loadFromFile(path, &serr)) return err("profile.verifyMapping", -32005, serr);
            }

            auto rep = build_validation_report(self, prof, self.hwmon(), withDetect, rpmMin);
            return ok("profile.verifyMapping", rep.dump());
        });

    reg.registerMethod("profile.import", "Import FanControl.Releases config; params:{path}", [&](const RpcRequest& rq) -> RpcResult {
        if (rq.params.empty()) return err("profile.import", -32602, "bad params");
        json p = json::parse(rq.params, nullptr, false);
        if (p.is_discarded() || !p.contains("path")) return err("profile.import", -32602, "missing path");

        std::string path = p["path"].get<std::string>();
        std::string e;
        Profile prof;
        if (!FanControlImport::LoadAndMap(path, self.hwmon(), prof, e)) return err("profile.import", -32010, e);

        (void)self.applyProfile(prof);

        DaemonConfig nc = self.cfg();
        nc.profileName = prof.name.empty() ? std::string("Imported") : prof.name;
        std::string serr;
        (void)saveDaemonConfig(nc, self.configPath(), &serr);
        self.setCfg(nc);

        return ok("profile.import", "{}");
    });

    reg.registerMethod("profile.importAs",
        "Import FanControl.Releases and save; params:{path,name,validateDetect?:bool,rpmMin?:int}",
        [&](const RpcRequest& rq) -> RpcResult {
            if (rq.params.empty()) return err("profile.importAs", -32602, "bad params");
            json p = json::parse(rq.params, nullptr, false);
            if (p.is_discarded() || !p.contains("path") || !p.contains("name"))
                return err("profile.importAs", -32602, "missing path or name");

            const std::string srcPath = p["path"].get<std::string>();
            std::string nameOnly = to_profile_name(p["name"].get<std::string>());
            if (nameOnly.empty()) return err("profile.importAs", -32602, "empty name");

            const bool withDetect = p.value("validateDetect", true);
            const int rpmMin = clampi(p.value("rpmMin", 300), 0, 10000);

            Profile prof;
            std::string ierr;
            if (!FanControlImport::LoadAndMap(srcPath, self.hwmon(), prof, ierr))
                return err("profile.importAs", -32010, ierr);

            prof.name = nameOnly;

            auto rep = build_validation_report(self, prof, self.hwmon(), withDetect, rpmMin);
            const bool verifyOk = rep.value("ok", false);

            json data;
            data["name"] = nameOnly;
            data["applied"] = false;
            data["warnings"] = inv_warnings(self.hwmon());
            data["verify"] = rep;

            if (!verifyOk) {
                return ok("profile.importAs", data.dump()); // do NOT save/apply on failed verification
            }

            const std::string dir = self.cfg().profilesDir;
            if (dir.empty()) return err("profile.importAs", -32011, "profilesDir not configured");

            std::error_code ec;
            std::filesystem::create_directories(dir, ec);

            auto dst = file_for_profile(dir, nameOnly);
            std::string serr;
            if (!prof.saveToFile(dst.string(), &serr))
                return err("profile.importAs", -32012, serr);

            if (!self.applyProfile(prof))
                return err("profile.importAs", -32013, "apply failed");

            DaemonConfig nc = self.cfg();
            nc.profileName = nameOnly;
            (void)saveDaemonConfig(nc, self.configPath(), &serr);
            self.setCfg(nc);

            data["savedPath"] = dst.string();
            data["applied"] = true;
            return ok("profile.importAs", data.dump());
        });

    reg.registerMethod("profile.load", "Load and apply profile by name; params:{name}", [&](const RpcRequest& rq) -> RpcResult {
        if (rq.params.empty()) return err("profile.load", -32602, "bad params");
        json p = json::parse(rq.params, nullptr, false);
        if (p.is_discarded() || !p.contains("name")) return err("profile.load", -32602, "missing name");

        std::string nameOnly = to_profile_name(p["name"].get<std::string>());
        auto full = file_for_profile(self.cfg().profilesDir, nameOnly);
        if (!std::filesystem::exists(full)) return err("profile.load", -32004, "profile not found");
        if (!self.applyProfileFile(full.string())) return err("profile.load", -32005, "invalid profile");

        DaemonConfig nc = self.cfg();
        nc.profileName = nameOnly;
        std::string serr;
        (void)saveDaemonConfig(nc, self.configPath(), &serr);
        self.setCfg(nc);

        return ok("profile.load", "{}");
    });

    reg.registerMethod("profile.set", "Write profile JSON; params:{name, profile}", [&](const RpcRequest& rq) -> RpcResult {
        if (rq.params.empty()) return err("profile.set", -32602, "bad params");
        json p = json::parse(rq.params, nullptr, false);
        if (p.is_discarded() || !p.contains("name") || !p.contains("profile")) return err("profile.set", -32602, "missing name/profile");

        std::string nameOnly = to_profile_name(p["name"].get<std::string>());
        json prof = p["profile"];

        auto path = file_for_profile(self.cfg().profilesDir, nameOnly);
        std::error_code ec;
        std::filesystem::create_directories(path.parent_path(), ec);
        if (ec) return err("profile.set", -32006, ec.message());

        std::ofstream f(path);
        if (!f) return err("profile.set", -32006, "open failed");
        f << prof.dump(2) << "\n";
        f.close();
        return ok("profile.set", "{}");
    });

    reg.registerMethod("profile.delete", "Delete profile file; params:{name}", [&](const RpcRequest& rq) -> RpcResult {
        if (rq.params.empty()) return err("profile.delete", -32602, "bad params");
        json p = json::parse(rq.params, nullptr, false);
        if (p.is_discarded() || !p.contains("name")) return err("profile.delete", -32602, "missing name");

        std::string nameOnly = to_profile_name(p["name"].get<std::string>());
        auto path = file_for_profile(self.cfg().profilesDir, nameOnly);
        std::error_code ec;
        bool okDel = std::filesystem::remove(path, ec);
        if (!okDel && ec) return err("profile.delete", -32007, ec.message());
        return ok("profile.delete", "{}");
    });

    // telemetry / daemon
    reg.registerMethod("telemetry.json", "Return current SHM JSON blob", [&](const RpcRequest&) -> RpcResult {
        std::string s;
        if (!self.telemetryGet(s)) s = "{}";
        return ok("telemetry.json", s);
    });
    reg.registerMethod("daemon.update", "Check/download latest release; params:{download?:bool,target?:string,repo?:\"owner/repo\"}", [&](const RpcRequest& rq) -> RpcResult {
        std::string repoOwner = "meigrafd";
        std::string repo = "LinuxFanControl";

        if (!rq.params.empty()) {
            json p = json::parse(rq.params, nullptr, false);
            if (!p.is_discarded() && p.contains("repo") && p["repo"].is_string()) {
                auto s = p["repo"].get<std::string>();
                auto pos = s.find('/');
                if (pos != std::string::npos) { repoOwner = s.substr(0, pos); repo = s.substr(pos + 1); }
            }
        }

        ReleaseInfo rel;
        std::string e;
        if (!UpdateChecker::fetchLatest(repoOwner, repo, rel, e)) return err("daemon.update", -32020, e);
        int cmp = UpdateChecker::compareVersions(LFCD_VERSION, rel.tag);

        json out;
        out["current"] = LFCD_VERSION;
        out["latest"]  = rel.tag;
        out["name"]    = rel.name;
        out["html"]    = rel.htmlUrl;
        out["updateAvailable"] = (cmp < 0);
        out["assets"]  = json::array();
        for (auto& a : rel.assets)
            out["assets"].push_back({{"name", a.name}, {"url", a.url}, {"type", a.type}, {"size", a.size}});

        bool dl = false; std::string target;
        if (!rq.params.empty()) {
            json p = json::parse(rq.params, nullptr, false);
            if (!p.is_discarded()) {
                dl = p.value("download", false);
                if (p.contains("target") && p["target"].is_string()) target = p["target"].get<std::string>();
            }
        }
        if (dl) {
            if (target.empty()) return err("daemon.update", -32602, "missing target");
            std::string url;
            for (const auto& a : rel.assets) {
                std::string n = a.name; for (auto& c : n) c = (char)tolower(c);
                if (n.find("linux") != std::string::npos &&
                    (n.find("x86_64") != std::string::npos || n.find("amd64") != std::string::npos)) { url = a.url; break; }
            }
            if (url.empty() && !rel.assets.empty()) url = rel.assets[0].url;
            if (url.empty()) return err("daemon.update", -32021, "no assets");
            if (!UpdateChecker::downloadToFile(url, target, e)) return err("daemon.update", -32022, e);
            out["downloaded"] = true; out["target"] = target;
        }
        return ok("daemon.update", out.dump());
    });
    reg.registerMethod("daemon.restart", "Request daemon restart", [&](const RpcRequest&) -> RpcResult {
        return ok("daemon.restart", "{}");
    });
    reg.registerMethod("daemon.shutdown", "Shutdown daemon gracefully", [&](const RpcRequest&) -> RpcResult {
        self.requestStop();
        return ok("daemon.shutdown", "{}");
    });
}

} // namespace lfc
