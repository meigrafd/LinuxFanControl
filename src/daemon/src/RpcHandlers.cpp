/*
 * Linux Fan Control — RPC handlers (implementation)
 * - Uniform responses: {"method":"...", "success":true/false, ...}
 * - Includes config.set for engine.deltaC / engine.forceTickMs
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
#include <cctype>

using nlohmann::json;

namespace lfc {

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

void BindDaemonRpcCommands(Daemon& self, CommandRegistry& reg) {
    // meta
    reg.registerMethod("ping", "Health check", [&](const RpcRequest&) -> RpcResult {
        return ok("ping", "{}");
    });

    reg.registerMethod("version", "RPC and daemon version", [&](const RpcRequest&) -> RpcResult {
        json j;
        j["daemon"] = "lfcd";
        j["version"] = LFC_VERSION;
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

    // config
    reg.registerMethod("config.load", "Load daemon config from disk", [&](const RpcRequest&) -> RpcResult {
        DaemonConfig tmp;
        std::string e;
        if (!Config::Load(self.configPath(), tmp, e)) return err("config.load", -32002, "config load failed");
        json out;
        out["log"]["file"] = tmp.log.file;
        out["log"]["maxBytes"] = static_cast<std::uint64_t>(tmp.log.maxBytes);
        out["log"]["rotateCount"] = tmp.log.rotateCount;
        out["log"]["debug"] = tmp.log.debug;
        out["rpc"]["host"] = tmp.rpc.host;
        out["rpc"]["port"] = tmp.rpc.port;
        out["shm"]["path"] = tmp.shm.path;
        out["profiles"]["dir"] = tmp.profiles.dir;
        out["profiles"]["active"] = tmp.profiles.active;
        out["profiles"]["backups"] = tmp.profiles.backups;
        out["pidFile"] = tmp.pidFile;
        // runtime engine knobs are exposed here too
        out["engine"]["deltaC"] = self.engineDeltaC();
        out["engine"]["forceTickMs"] = self.engineForceTickMs();
        return ok("config.load", out.dump());
    });

    reg.registerMethod("config.save", "Save daemon config to disk; params:{config:object}", [&](const RpcRequest& rq) -> RpcResult {
        json p;
        try { p = json::parse(rq.params.empty() ? "{}" : rq.params); }
        catch (...) { return err("config.save", -32602, "bad params"); }
        if (!p.contains("config") || !p["config"].is_object()) return err("config.save", -32602, "missing config");

        DaemonConfig nc = self.cfg();
        auto& jc = p["config"];
        if (jc.contains("log")) {
            nc.log.file = jc["log"].value("file", nc.log.file);
            nc.log.maxBytes = static_cast<std::size_t>(jc["log"].value("maxBytes", static_cast<std::uint64_t>(nc.log.maxBytes)));
            nc.log.rotateCount = jc["log"].value("rotateCount", nc.log.rotateCount);
            nc.log.debug = jc["log"].value("debug", nc.log.debug);
        }
        if (jc.contains("rpc")) {
            nc.rpc.host = jc["rpc"].value("host", nc.rpc.host);
            nc.rpc.port = jc["rpc"].value("port", nc.rpc.port);
        }
        if (jc.contains("shm")) {
            nc.shm.path = jc["shm"].value("path", nc.shm.path);
        }
        if (jc.contains("profiles")) {
            nc.profiles.dir = jc["profiles"].value("dir", nc.profiles.dir);
            nc.profiles.active = jc["profiles"].value("active", nc.profiles.active);
            nc.profiles.backups = jc["profiles"].value("backups", nc.profiles.backups);
        }
        if (jc.contains("pidFile")) {
            nc.pidFile = jc.value("pidFile", nc.pidFile);
        }

        // runtime-only engine knobs (not persisted by Config::Save unless du speicherst sie dort explizit)
        if (jc.contains("engine") && jc["engine"].is_object()) {
            if (jc["engine"].contains("deltaC") && jc["engine"]["deltaC"].is_number()) {
                self.setEngineDeltaC(jc["engine"]["deltaC"].get<double>());
            }
            if (jc["engine"].contains("forceTickMs") && jc["engine"]["forceTickMs"].is_number_integer()) {
                self.setEngineForceTickMs(jc["engine"]["forceTickMs"].get<int>());
            }
        }

        std::string e;
        if (!Config::Save(self.configPath(), nc, e)) return err("config.save", -32003, "config save failed");
        self.setCfg(nc);
        return ok("config.save", "{}");
    });

    reg.registerMethod("config.set", "Set single key; params:{key:string, value:any}", [&](const RpcRequest& rq) -> RpcResult {
        json p;
        try { p = json::parse(rq.params.empty() ? "{}" : rq.params); }
        catch (...) { return err("config.set", -32602, "bad params"); }
        if (!p.contains("key")) return err("config.set", -32602, "missing key");

        std::string key = p["key"].get<std::string>();
        json val = p.contains("value") ? p["value"] : json();

        DaemonConfig nc = self.cfg();

        if (key == "log.debug") {
            if (val.is_boolean()) nc.log.debug = val.get<bool>();
        } else if (key == "log.file") {
            if (val.is_string()) nc.log.file = val.get<std::string>();
        } else if (key == "rpc.host") {
            if (val.is_string()) nc.rpc.host = val.get<std::string>();
        } else if (key == "rpc.port") {
            if (val.is_number_integer()) nc.rpc.port = val.get<int>();
        } else if (key == "shm.path") {
            if (val.is_string()) nc.shm.path = val.get<std::string>();
        } else if (key == "profiles.dir") {
            if (val.is_string()) nc.profiles.dir = val.get<std::string>();
        } else if (key == "profiles.active") {
            if (val.is_string()) nc.profiles.active = val.get<std::string>();
        } else if (key == "pidFile") {
            if (val.is_string()) nc.pidFile = val.get<std::string>();
        } else if (key == "engine.deltaC") {
            if (val.is_number()) self.setEngineDeltaC(val.get<double>());
            return ok("config.set", "{}");
        } else if (key == "engine.forceTickMs") {
            if (val.is_number_integer()) self.setEngineForceTickMs(val.get<int>());
            return ok("config.set", "{}");
        } else {
            return err("config.set", -32602, "unknown key");
        }

        std::string e;
        if (!Config::Save(self.configPath(), nc, e)) return err("config.set", -32003, "config save failed");
        self.setCfg(nc);
        return ok("config.set", "{}");
    });

    // profiles
    reg.registerMethod("profile.import", "Import FanControl.Releases config; params:{path}", [&](const RpcRequest& rq) -> RpcResult {
        json p;
        try { p = json::parse(rq.params.empty() ? "{}" : rq.params); }
        catch (...) { return err("profile.import", -32602, "bad params"); }
        if (!p.contains("path")) return err("profile.import", -32602, "missing path");
        std::string path = p["path"].get<std::string>();

        std::string e;
        Profile prof;
        if (!FanControlImport::LoadAndMap(path, self.hwmon(), prof, e)) return err("profile.import", -32010, e);
        self.engine().applyProfile(prof);
        return ok("profile.import", "{}");
    });

    reg.registerMethod("profile.load", "Load and apply profile by name; params:{name}", [&](const RpcRequest& rq) -> RpcResult {
        json p;
        try { p = json::parse(rq.params.empty() ? "{}" : rq.params); }
        catch (...) { return err("profile.load", -32602, "bad params"); }
        if (!p.contains("name")) return err("profile.load", -32602, "missing name");
        std::string name = p["name"].get<std::string>();
        std::filesystem::path full = std::filesystem::path(self.cfg().profiles.dir) / name;
        if (!std::filesystem::exists(full)) return err("profile.load", -32004, "profile not found");
        if (!self.applyProfileFile(full.string())) return err("profile.load", -32005, "invalid profile");
        DaemonConfig nc = self.cfg();
        nc.profiles.active = name;
        std::string serr;
        (void)Config::Save(self.configPath(), nc, serr);
        self.setCfg(nc);
        return ok("profile.load", "{}");
    });

    reg.registerMethod("profile.set", "Write profile JSON; params:{name, profile}", [&](const RpcRequest& rq) -> RpcResult {
        json p;
        try { p = json::parse(rq.params.empty() ? "{}" : rq.params); }
        catch (...) { return err("profile.set", -32602, "bad params"); }
        if (!p.contains("name") || !p.contains("profile")) return err("profile.set", -32602, "missing name/profile");
        std::string name = p["name"].get<std::string>();
        std::string tmp = p["profile"].dump();
        std::filesystem::path path = std::filesystem::path(self.cfg().profiles.dir) / name;
        std::error_code ec;
        std::filesystem::create_directories(path.parent_path(), ec);
        std::ofstream f(path);
        if (!f) return err("profile.set", -32006, "open failed");
        f << tmp << "\n";
        f.close();
        return ok("profile.set", "{}");
    });

    reg.registerMethod("profile.delete", "Delete profile file; params:{name}", [&](const RpcRequest& rq) -> RpcResult {
        json p;
        try { p = json::parse(rq.params.empty() ? "{}" : rq.params); }
        catch (...) { return err("profile.delete", -32602, "bad params"); }
        if (!p.contains("name")) return err("profile.delete", -32602, "missing name");
        std::filesystem::path full = std::filesystem::path(self.cfg().profiles.dir) / p["name"].get<std::string>();
        std::error_code ec;
        bool okrm = std::filesystem::remove(full, ec);
        if (!okrm || ec) return err("profile.delete", -32007, "delete failed");
        return ok("profile.delete", "{}");
    });

    reg.registerMethod("active.profile", "Get active profile filename", [&](const RpcRequest&) -> RpcResult {
        json d;
        d["active"] = self.cfg().profiles.active;
        return ok("active.profile", d.dump());
    });

    reg.registerMethod("list.profiles", "List profiles in profiles dir", [&](const RpcRequest&) -> RpcResult {
        json arr = json::array();
        std::error_code ec;
        for (auto& e : std::filesystem::directory_iterator(self.cfg().profiles.dir, ec)) {
            if (!e.is_regular_file()) continue;
            if (e.path().extension() == ".json") arr.push_back(e.path().filename().string());
        }
        return ok("list.profiles", arr.dump());
    });

    // detection
    reg.registerMethod("detect.start", "Start non-blocking detection", [&](const RpcRequest&) -> RpcResult {
        auto r = self.detectionStart();
        if (!r) return err("detect.start", -32030, "already running");
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
        auto v = self.detectionResults();
        json arr = json::array();
        const auto& hw = self.hwmon();
        for (size_t i = 0; i < v.size(); ++i) {
            json item;
            item["pwm"] = (i < hw.pwms.size() ? hw.pwms[i].path_pwm : std::string{});
            item["peak_rpm"] = v[i];
            arr.push_back(item);
        }
        return ok("detect.results", arr.dump());
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

    reg.registerMethod("engine.status", "Basic engine status", [&](const RpcRequest&) -> RpcResult {
        const auto& hw = self.hwmon();
        json d;
        d["control"] = self.engineControlEnabled();
        d["pwms"] = hw.pwms.size();
        d["fans"] = hw.fans.size();
        d["temps"] = hw.temps.size();
        d["deltaC"] = self.engineDeltaC();
        d["forceTickMs"] = self.engineForceTickMs();
        return ok("engine.status", d.dump());
    });

    reg.registerMethod("engine.reset", "Disable control and clear profile", [&](const RpcRequest&) -> RpcResult {
        self.engineEnable(false);
        Profile empty;
        self.engine().applyProfile(empty);
        return ok("engine.reset", "{}");
    });

    // hwmon / telemetry
    reg.registerMethod("hwmon.snapshot", "Counts of discovered devices", [&](const RpcRequest&) -> RpcResult {
        const auto& hw = self.hwmon();
        json d;
        d["pwms"] = hw.pwms.size();
        d["fans"] = hw.fans.size();
        d["temps"] = hw.temps.size();
        return ok("hwmon.snapshot", d.dump());
    });

    reg.registerMethod("list.sensor", "List temperature sensors", [&](const RpcRequest&) -> RpcResult {
        const auto& hw = self.hwmon();
        json arr = json::array();
        for (const auto& t : hw.temps) arr.push_back(t.label.empty() ? t.path_input : t.label);
        return ok("list.sensor", arr.dump());
    });

    reg.registerMethod("list.fan", "List fan tach inputs", [&](const RpcRequest&) -> RpcResult {
        const auto& hw = self.hwmon();
        json arr = json::array();
        for (const auto& f : hw.fans) arr.push_back(f.path_input);
        return ok("list.fan", arr.dump());
    });

    reg.registerMethod("list.pwm", "List PWM outputs", [&](const RpcRequest&) -> RpcResult {
        const auto& hw = self.hwmon();
        json arr = json::array();
        for (const auto& p : hw.pwms) arr.push_back(p.path_pwm);
        return ok("list.pwm", arr.dump());
    });

    reg.registerMethod("telemetry.json", "Return current SHM JSON blob", [&](const RpcRequest&) -> RpcResult {
        std::string s;
        if (!self.telemetryGet(s)) return err("telemetry.json", -32001, "no telemetry");
        return ok("telemetry.json", s.empty() ? "null" : s);
    });

    // update / lifecycle
    reg.registerMethod("daemon.update", "Check/download latest release; params:{download?:bool,target?:string,repo?:\"owner/repo\"}", [&](const RpcRequest& rq) -> RpcResult {
        std::string owner = "meigrafd", repo = "LinuxFanControl";
        json p;
        try {
            p = json::parse(rq.params.empty() ? "{}" : rq.params);
            if (p.contains("repo") && p["repo"].is_string()) {
                auto s = p["repo"].get<std::string>();
                auto k = s.find('/');
                if (k != std::string::npos) { owner = s.substr(0, k); repo = s.substr(k + 1); }
            }
        } catch (...) {}
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
        return ok("daemon.restart", "{}");
    });

    reg.registerMethod("daemon.shutdown", "Shutdown daemon gracefully", [&](const RpcRequest&) -> RpcResult {
        self.requestStop();
        return ok("daemon.shutdown", "{}");
    });
}

} // namespace lfc
