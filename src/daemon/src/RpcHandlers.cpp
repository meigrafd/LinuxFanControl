/*
 * Linux Fan Control — RPC handlers (implementation)
 * - Uniform responses: {"method":"...", "success":true/false, ...}
 * - Includes config.set for engine.{deltaC,forceTickMs,tickMs}
 * - list.sensor returns detailed sensor info (path, label, value, unit) with 2 decimals
 * - FanControlImport + UpdateChecker wired
 * (c) 2025 LinuxFanControl contributors
 */
#include "RpcHandlers.hpp"
#include "Daemon.hpp"
#include "include/CommandRegistry.h"
#include "Config.hpp"
#include "FanControlImport.hpp"
#include "UpdateChecker.hpp"
#include "Hwmon.hpp"
#include "Log.hpp"
#include "Version.hpp"

#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <cctype>

using nlohmann::json;

namespace lfc {

// ---------- helpers for uniform JSON-RPC-like response body ----------

static inline std::string jstr(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 2);
    out.push_back('"');
    for (char c : s) {
        if (c == '"' || c == '\\') out.push_back('\\');
        out.push_back(c);
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

static inline std::string fmt2(double v) {
    std::ostringstream oss;
    oss.setf(std::ios::fixed);
    oss << std::setprecision(2) << v;
    return oss.str();
}

static int clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }
static double clampd(double v, double lo, double hi) { return v < lo ? lo : (v > hi ? hi : v); }

// ---------- command bindings ----------

void BindDaemonRpcCommands(Daemon& self, CommandRegistry& reg) {
    // --- meta ---
    reg.registerMethod("ping", "Health check", [&](const RpcRequest&) -> RpcResult {
        return ok("ping", "{}");
    });

    reg.registerMethod("version", "RPC and daemon version", [&](const RpcRequest&) -> RpcResult {
        json j;
        j["daemon"] = "lfcd";
        j["version"] = LFC_VERSION;
        j["rpc"] = 1;
        return ok("version", j.dump());
    });

    reg.registerMethod("rpc.commands", "List available RPC methods", [&](const RpcRequest&) -> RpcResult {
        auto list = self.listRpcCommands();
        json arr = json::array();
        for (const auto& c : list) {
            arr.push_back({{"name", c.name}, {"help", c.help}});
        }
        return ok("rpc.commands", arr.dump());
    });

    // --- config (Mapping auf neue flache DaemonConfig, aber JSON wie gestern) ---

    reg.registerMethod("config.load", "Load daemon config from disk", [&](const RpcRequest&) -> RpcResult {
        std::string e;
        DaemonConfig tmp = loadDaemonConfig(self.configPath(), &e);
        if (!e.empty()) return err("config.load", -32002, "config load failed");

        json out;
        out["log"]["file"]           = tmp.logfile;
        out["log"]["maxBytes"]       = 5ull * 1024ull * 1024ull; // placeholder (rotation not implemented)
        out["log"]["rotateCount"]    = 3;                        // placeholder
        out["log"]["debug"]          = tmp.debug;

        out["rpc"]["host"]           = tmp.host;
        out["rpc"]["port"]           = tmp.port;

        out["shm"]["path"]           = tmp.shmPath;

        out["profiles"]["dir"]       = tmp.profilesDir;
        out["profiles"]["active"]    = tmp.profileName;
        out["profiles"]["backups"]   = true; // placeholder

        out["pidFile"]               = tmp.pidfile;

        out["engine"]["deltaC"]      = self.engineDeltaC();
        out["engine"]["forceTickMs"] = self.engineForceTickMs();
        out["engine"]["tickMs"]      = self.engineTickMs();

        return ok("config.load", out.dump());
    });

    reg.registerMethod("config.save", "Save daemon config to disk; params:{config:object}", [&](const RpcRequest& rq) -> RpcResult {
        if (rq.params.empty()) return err("config.save", -32602, "bad params");
        json p = json::parse(rq.params, nullptr, false);
        if (p.is_discarded() || !p.contains("config") || !p["config"].is_object()) {
            return err("config.save", -32602, "missing config");
        }

        DaemonConfig nc = self.cfg();
        auto& jc = p["config"];

        if (jc.contains("log")) {
            if (jc["log"].contains("file") && jc["log"]["file"].is_string()) {
                nc.logfile = jc["log"]["file"].get<std::string>();
            }
            if (jc["log"].contains("debug") && jc["log"]["debug"].is_boolean()) {
                nc.debug = jc["log"]["debug"].get<bool>();
            }
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
            if (jc["profiles"].contains("active") && jc["profiles"]["active"].is_string()) nc.profileName = jc["profiles"]["active"].get<std::string>();
        }
        if (jc.contains("pidFile") && jc["pidFile"].is_string()) {
            nc.pidfile = jc["pidFile"].get<std::string>();
        }
        if (jc.contains("engine") && jc["engine"].is_object()) {
            if (jc["engine"].contains("deltaC") && jc["engine"]["deltaC"].is_number()) {
                double v = clampd(jc["engine"]["deltaC"].get<double>(), 0.0, 10.0);
                nc.deltaC = v;
                self.setEngineDeltaC(v);
            }
            if (jc["engine"].contains("forceTickMs") && jc["engine"]["forceTickMs"].is_number_integer()) {
                int v = clampi(jc["engine"]["forceTickMs"].get<int>(), 100, 10000);
                nc.forceTickMs = v;
                self.setEngineForceTickMs(v);
            }
            if (jc["engine"].contains("tickMs") && jc["engine"]["tickMs"].is_number_integer()) {
                int v = clampi(jc["engine"]["tickMs"].get<int>(), 5, 1000);
                nc.tickMs = v;
                self.setEngineTickMs(v);
            }
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
            if (val.is_string()) nc.profileName = val.get<std::string>();
        } else if (key == "pidFile") {
            if (val.is_string()) nc.pidfile = val.get<std::string>();
        } else if (key == "engine.deltaC") {
            double v = self.engineDeltaC();
            if (val.is_number()) v = val.get<double>();
            else if (val.is_string()) { try { v = std::stod(val.get<std::string>()); } catch (...) {} }
            v = clampd(v, 0.0, 10.0);
            nc.deltaC = v;
            self.setEngineDeltaC(v);
            // save below
        } else if (key == "engine.forceTickMs") {
            int v = self.engineForceTickMs();
            if (val.is_number_integer()) v = val.get<int>();
            else if (val.is_string()) { try { v = std::stoi(val.get<std::string>()); } catch (...) {} }
            v = clampi(v, 100, 10000);
            nc.forceTickMs = v;
            self.setEngineForceTickMs(v);
        } else if (key == "engine.tickMs") {
            int v = self.engineTickMs();
            if (val.is_number_integer()) v = val.get<int>();
            else if (val.is_string()) { try { v = std::stoi(val.get<std::string>()); } catch (...) {} }
            v = clampi(v, 5, 1000);
            nc.tickMs = v;
            self.setEngineTickMs(v);
        } else {
            return err("config.set", -32602, "unknown key");
        }

        std::string e;
        if (!saveDaemonConfig(nc, self.configPath(), &e)) return err("config.set", -32003, e);
        self.setCfg(nc);
        return ok("config.set", "{}");
    });

    // --- profiles ---

    reg.registerMethod("profile.import", "Import FanControl.Releases config; params:{path}", [&](const RpcRequest& rq) -> RpcResult {
        if (rq.params.empty()) return err("profile.import", -32602, "bad params");
        json p = json::parse(rq.params, nullptr, false);
        if (p.is_discarded() || !p.contains("path")) return err("profile.import", -32602, "missing path");

        std::string path = p["path"].get<std::string>();
        std::string e;
        Profile prof;
        if (!FanControlImport::LoadAndMap(path, self.hwmon(), prof, e)) return err("profile.import", -32010, e);
        self.engine().applyProfile(prof);
        return ok("profile.import", "{}");
    });

    reg.registerMethod("profile.load", "Load and apply profile by name; params:{name}", [&](const RpcRequest& rq) -> RpcResult {
        if (rq.params.empty()) return err("profile.load", -32602, "bad params");
        json p = json::parse(rq.params, nullptr, false);
        if (p.is_discarded() || !p.contains("name")) return err("profile.load", -32602, "missing name");

        std::string name = p["name"].get<std::string>();
        std::filesystem::path full = std::filesystem::path(self.cfg().profilesDir) / name;
        if (!std::filesystem::exists(full)) return err("profile.load", -32004, "profile not found");
        if (!self.applyProfileFile(full.string())) return err("profile.load", -32005, "invalid profile");

        DaemonConfig nc = self.cfg();
        nc.profileName = name;
        std::string serr;
        (void)saveDaemonConfig(nc, self.configPath(), &serr);
        self.setCfg(nc);

        return ok("profile.load", "{}");
    });

    reg.registerMethod("profile.set", "Write profile JSON; params:{name, profile}", [&](const RpcRequest& rq) -> RpcResult {
        if (rq.params.empty()) return err("profile.set", -32602, "bad params");
        json p = json::parse(rq.params, nullptr, false);
        if (p.is_discarded() || !p.contains("name") || !p.contains("profile")) return err("profile.set", -32602, "missing name/profile");

        std::string name = p["name"].get<std::string>();
        json prof = p["profile"];

        std::filesystem::path path = std::filesystem::path(self.cfg().profilesDir) / name;
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

        std::filesystem::path full = std::filesystem::path(self.cfg().profilesDir) / p["name"].get<std::string>();
        std::error_code ec;
        bool okrm = std::filesystem::remove(full, ec);
        if (!okrm || ec) return err("profile.delete", -32007, "delete failed");
        return ok("profile.delete", "{}");
    });

    reg.registerMethod("active.profile", "Get active profile filename", [&](const RpcRequest&) -> RpcResult {
        json d; d["active"] = self.cfg().profileName;
        return ok("active.profile", d.dump());
    });

    reg.registerMethod("list.profiles", "List profiles in profiles dir", [&](const RpcRequest&) -> RpcResult {
        json arr = json::array();
        std::error_code ec;
        for (auto& e : std::filesystem::directory_iterator(self.cfg().profilesDir, ec)) {
            if (!e.is_regular_file()) continue;
            if (e.path().extension() == ".json") arr.push_back(e.path().filename().string());
        }
        return ok("list.profiles", arr.dump());
    });

    // --- detection ---

    reg.registerMethod("detect.start", "Start non-blocking detection", [&](const RpcRequest&) -> RpcResult {
        if (!self.detectionStart()) return err("detect.start", -32030, "already running");
        return ok("detect.start", "{}");
    });

    reg.registerMethod("detect.status", "Detection status/progress", [&](const RpcRequest&) -> RpcResult {
        auto st = self.detectionStatus();
        json d;
        d["running"] = st.running;
        d["current_index"] = st.currentIndex;
        d["total"] = st.total;
        d["phase"] = st.phase;
        return ok("detect.status", d.dump());
    });

    reg.registerMethod("detect.abort", "Abort detection", [&](const RpcRequest&) -> RpcResult {
        self.detectionAbort();
        return ok("detect.abort", "{}");
    });

    reg.registerMethod("detect.results", "Return detection peak RPMs per PWM", [&](const RpcRequest&) -> RpcResult {
        auto peaks = self.detectionResults();
        json arr = json::array();
        const auto& hw = self.hwmon();
        for (size_t i = 0; i < peaks.size(); ++i) {
            json item;
            item["pwm"] = (i < hw.pwms.size() ? hw.pwms[i].path_pwm : std::string{});
            item["peak_rpm"] = peaks[i];
            arr.push_back(item);
        }
        return ok("detect.results", arr.dump());
    });

    // --- engine ---

    reg.registerMethod("engine.enable", "Enable automatic control", [&](const RpcRequest&) -> RpcResult {
        self.engineEnable(true);
        return ok("engine.enable", "{}");
    });

    reg.registerMethod("engine.disable", "Disable automatic control", [&](const RpcRequest&) -> RpcResult {
        self.engineEnable(false);
        return ok("engine.disable", "{}");
    });

    reg.registerMethod("engine.status", "Basic engine status", [&](const RpcRequest&) -> RpcResult {
        const auto& hw = self.hwmon();
        json d;
        d["control"] = self.engineControlEnabled();
        d["pwms"] = hw.pwms.size();
        d["fans"] = hw.fans.size();
        d["temps"] = hw.temps.size();
        d["deltaC"] = self.engineDeltaC();
        d["forceTickMs"] = self.engineForceTickMs();
        d["tickMs"] = self.engineTickMs();
        return ok("engine.status", d.dump());
    });

    reg.registerMethod("engine.reset", "Disable control and clear profile", [&](const RpcRequest&) -> RpcResult {
        self.engineEnable(false);
        Profile empty;
        self.engine().applyProfile(empty);
        return ok("engine.reset", "{}");
    });

    // --- hwmon / telemetry ---

    reg.registerMethod("hwmon.snapshot", "Counts of discovered devices", [&](const RpcRequest&) -> RpcResult {
        const auto& hw = self.hwmon();
        json d;
        d["pwms"]  = hw.pwms.size();
        d["fans"]  = hw.fans.size();
        d["temps"] = hw.temps.size();
        return ok("hwmon.snapshot", d.dump());
    });

    reg.registerMethod("list.sensor", "List temperature sensors with current values", [&](const RpcRequest&) -> RpcResult {
        const auto& hw = self.hwmon();
        json arr = json::array();
        for (const auto& t : hw.temps) {
            json obj;
            obj["path"] = t.path_input;
            obj["label"] = t.label.empty() ? t.path_input : t.label;
            auto mC = Hwmon::readMilliC(t);
            if (mC.has_value()) {
                double c = static_cast<double>(*mC) / 1000.0;
                obj["value"] = fmt2(c);
            } else {
                obj["value"] = nullptr;
            }
            obj["unit"] = "C";
            arr.push_back(obj);
        }
        return ok("list.sensor", arr.dump());
    });

    reg.registerMethod("list.fan", "List fan tach inputs", [&](const RpcRequest&) -> RpcResult {
        const auto& hw = self.hwmon();
        json arr = json::array();
        for (const auto& f : hw.fans) {
            json obj;
            obj["path"] = f.path_input;
            obj["rpm"] = Hwmon::readRpm(f).value_or(0);
            arr.push_back(obj);
        }
        return ok("list.fan", arr.dump());
    });

    reg.registerMethod("list.pwm", "List PWM outputs", [&](const RpcRequest&) -> RpcResult {
        const auto& hw = self.hwmon();
        json arr = json::array();
        for (const auto& p : hw.pwms) {
            json obj;
            obj["path"] = p.path_pwm;
            obj["percent"] = Hwmon::readPercent(p).value_or(-1);
            arr.push_back(obj);
        }
        return ok("list.pwm", arr.dump());
    });

    reg.registerMethod("telemetry.json", "Return current SHM JSON blob", [&](const RpcRequest&) -> RpcResult {
        std::string s;
        if (!self.telemetryGet(s)) return err("telemetry.json", -32001, "no telemetry");
        return ok("telemetry.json", s.empty() ? "null" : s);
    });

    // --- update / lifecycle ---

    reg.registerMethod("daemon.update", "Check/download latest release; params:{download?:bool,target?:string,repo?:\"owner/repo\"}", [&](const RpcRequest& rq) -> RpcResult {
        std::string owner = "meigrafd", repo = "LinuxFanControl";
        json p = json::parse(rq.params.empty() ? "{}" : rq.params, nullptr, false);
        if (!p.is_discarded() && p.contains("repo") && p["repo"].is_string()) {
            auto s = p["repo"].get<std::string>();
            auto k = s.find('/');
            if (k != std::string::npos) { owner = s.substr(0, k); repo = s.substr(k + 1); }
        }

        ReleaseInfo rel;
        std::string e;
        if (!UpdateChecker::fetchLatest(owner, repo, rel, e)) return err("daemon.update", -32020, e);
        int cmp = UpdateChecker::compareVersions(LFC_VERSION, rel.tag);

        json out;
        out["current"] = LFC_VERSION;
        out["latest"] = rel.tag;
        out["name"] = rel.name;
        out["html"] = rel.htmlUrl;
        out["updateAvailable"] = (cmp < 0);
        out["assets"] = json::array();
        for (auto& a : rel.assets) {
            out["assets"].push_back({{"name", a.name}, {"url", a.url}, {"type", a.type}, {"size", a.size}});
        }

        bool dl = p.value("download", false);
        if (dl) {
            std::string target = p.value("target", std::string());
            if (target.empty()) return err("daemon.update", -32602, "missing target");
            std::string url;
            for (const auto& a : rel.assets) {
                std::string n = a.name;
                for (auto& c : n) c = static_cast<char>(tolower(c));
                if (n.find("linux") != std::string::npos &&
                    (n.find("x86_64") != std::string::npos || n.find("amd64") != std::string::npos)) {
                    url = a.url;
                    break;
                }
            }
            if (url.empty() && !rel.assets.empty()) url = rel.assets[0].url;
            if (url.empty()) return err("daemon.update", -32021, "no assets");
            if (!UpdateChecker::downloadToFile(url, target, e)) return err("daemon.update", -32022, e);
            out["downloaded"] = true;
            out["target"] = target;
        }

        return ok("daemon.update", out.dump());
    });

    reg.registerMethod("daemon.restart", "Request daemon restart", [&](const RpcRequest&) -> RpcResult {
        // Supervisor/systemd should do the restart; we just ACK.
        return ok("daemon.restart", "{}");
    });

    reg.registerMethod("daemon.shutdown", "Shutdown daemon gracefully", [&](const RpcRequest&) -> RpcResult {
        self.requestStop();
        return ok("daemon.shutdown", "{}");
    });
}

} // namespace lfc
