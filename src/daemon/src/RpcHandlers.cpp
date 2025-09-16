/*
 * Linux Fan Control — RPC handlers
 * (c) 2025 LinuxFanControl contributors
 */
#include "RpcHandlers.hpp"
#include "Daemon.hpp"
#include "Hwmon.hpp"
#include "Config.hpp"
#include "Profile.hpp"
#include "include/CommandRegistry.h"

#include <nlohmann/json.hpp>
#include <filesystem>
#include <string>

using nlohmann::json;

namespace lfc {

static RpcResult ok(const std::string& method, const json& data = json::object()) {
    json r;
    r["ok"] = true;
    r["method"] = method;
    r["data"] = data;
    return RpcResult{true, r.dump()};
}

static RpcResult err(const std::string& method, int code, const std::string& msg) {
    json r;
    r["ok"] = false;
    r["method"] = method;
    r["code"] = code;
    r["error"] = msg;
    return RpcResult{false, r.dump()};
}

void BindDaemonRpcCommands(Daemon& self, CommandRegistry& reg) {
    reg.registerMethod("config.get", "Return current daemon config", [&](const RpcRequest&) -> RpcResult {
        std::string e;
        DaemonConfig tmp = loadDaemonConfig(self.configPath(), &e);
        if (!e.empty()) return err("config.get", -32002, e);

        json out;
        out["logLevel"]        = tmp.logLevel;
        out["debug"]           = tmp.debug;
        out["foreground"]      = tmp.foreground;
        out["cmds"]            = tmp.cmds;
        out["host"]            = tmp.host;
        out["port"]            = tmp.port;
        out["pidfile"]         = tmp.pidfile;
        out["logfile"]         = tmp.logfile;
        out["shm"]             = tmp.shmPath;
        out["sysfsRoot"]       = tmp.sysfsRoot;
        out["sensorsBackend"]  = tmp.sensorsBackend;
        out["profilesDir"]     = tmp.profilesDir;
        out["profile"]         = tmp.profileName;

        return ok("config.get", out);
    });

    reg.registerMethod("config.set", "Mutate and persist daemon config", [&](const RpcRequest& rq) -> RpcResult {
        const std::string& params = rq.params;
        if (params.empty()) return err("config.set", -32602, "missing params");
        json jc = json::parse(params, nullptr, false);
        if (jc.is_discarded()) return err("config.set", -32602, "invalid json");

        DaemonConfig nc = self.cfg();

        if (jc.contains("logLevel"))       nc.logLevel       = jc.value("logLevel", nc.logLevel);
        if (jc.contains("debug"))          nc.debug          = jc.value("debug", nc.debug);
        if (jc.contains("foreground"))     nc.foreground     = jc.value("foreground", nc.foreground);
        if (jc.contains("cmds"))           nc.cmds           = jc.value("cmds", nc.cmds);

        if (jc.contains("host"))           nc.host           = jc.value("host", nc.host);
        if (jc.contains("port"))           nc.port           = jc.value("port", nc.port);

        if (jc.contains("pidfile"))        nc.pidfile        = jc.value("pidfile", nc.pidfile);
        if (jc.contains("logfile"))        nc.logfile        = jc.value("logfile", nc.logfile);
        if (jc.contains("shm"))            nc.shmPath        = jc.value("shm", nc.shmPath);

        if (jc.contains("sysfsRoot"))      nc.sysfsRoot      = jc.value("sysfsRoot", nc.sysfsRoot);
        if (jc.contains("sensorsBackend")) nc.sensorsBackend = jc.value("sensorsBackend", nc.sensorsBackend);

        if (jc.contains("profilesDir"))    nc.profilesDir    = jc.value("profilesDir", nc.profilesDir);
        if (jc.contains("profile"))        nc.profileName    = jc.value("profile", nc.profileName);

        std::string e;
        if (!saveDaemonConfig(nc, self.configPath(), &e)) return err("config.set", -32003, e);

        self.setCfg(nc);
        return ok("config.set");
    });

    reg.registerMethod("profile.activate", "Activate a profile by file name", [&](const RpcRequest& rq) -> RpcResult {
        const std::string& params = rq.params;
        if (params.empty()) return err("profile.activate", -32602, "missing params");
        json p = json::parse(params, nullptr, false);
        if (p.is_discarded() || !p.contains("name")) return err("profile.activate", -32602, "param 'name' required");
        std::string name = p["name"].get<std::string>();

        std::filesystem::path full = std::filesystem::path(self.cfg().profilesDir) / name;
        if (!self.applyProfileFile(full.string())) return err("profile.activate", -32004, "apply failed");

        DaemonConfig nc = self.cfg();
        nc.profileName = name;
        std::string serr;
        (void)saveDaemonConfig(nc, self.configPath(), &serr);
        self.setCfg(nc);

        return ok("profile.activate");
    });

    reg.registerMethod("profile.list", "List available profiles in profilesDir", [&](const RpcRequest&) -> RpcResult {
        json arr = json::array();
        std::error_code ec;
        for (auto& e : std::filesystem::directory_iterator(self.cfg().profilesDir, ec)) {
            if (ec) break;
            if (!e.is_regular_file()) continue;
            auto path = e.path();
            if (path.extension() == ".json") {
                json item;
                item["name"] = path.filename().string();
                item["path"] = path.string();
                arr.push_back(item);
            }
        }
        json d;
        d["profiles"] = arr;
        d["active"] = self.cfg().profileName;
        return ok("profile.list", d);
    });

    reg.registerMethod("telemetry.json", "Return current SHM JSON blob", [&](const RpcRequest&) -> RpcResult {
        std::string s;
        if (!self.telemetryGet(s)) return err("telemetry.json", -32001, "no telemetry");
        return ok("telemetry.json", s.empty() ? "null" : s);
    });
}

} // namespace lfc
