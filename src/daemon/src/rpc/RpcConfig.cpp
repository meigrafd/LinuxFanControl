/*
 * Linux Fan Control — RPC: Configuration endpoints
 * (c) 2025 LinuxFanControl contributors
 */

#include <nlohmann/json.hpp>
#include <string>

#include "include/CommandRegistry.hpp"
#include "include/CommandIntrospection.hpp"
#include "include/Version.hpp"
#include "include/Log.hpp"
#include "include/Config.hpp"
#include "include/Daemon.hpp"

namespace lfc {

using nlohmann::json;

static inline RpcResult ok_(const RpcRequest& rq, const char* method, const json& data = json::object()) {
    return RpcResult::makeOk(
        rq.id,
        json{
            {"method",  method},
            {"success", true},
            {"data",    data}
        }
    );
}

static inline RpcResult err_(const RpcRequest& rq, const char* method, int code, const std::string& msg, const json& data = json::object()) {
    (void)method;
    return RpcResult::makeError(rq.id, code, msg, data);
}

static inline std::string paramsToString_(const json& p) {
    if (p.is_string()) return p.get<std::string>();
    if (p.is_null())   return "{}";
    return p.dump();
}

void BindRpcConfig(Daemon& self, CommandRegistry& reg) {
    reg.add(
        "config.get",
        "Get effective daemon configuration",
        [&](const RpcRequest& rq) -> RpcResult {
            try {
                DaemonConfig cfg = defaultConfig();

                std::string path = cfg.configFile;
                if (!path.empty()) {
                    try {
                        loadDaemonConfig(path, cfg);
                    } catch (const std::exception& ex) {
                        LOG_WARN("config.get: load failed from '%s': %s", path.c_str(), ex.what());
                        return ok_(rq, "config.get", json{{"config", cfg}, {"loaded", false}, {"error", ex.what()}});
                    }
                }
                return ok_(rq, "config.get", json{{"config", cfg}, {"loaded", true}});
            } catch (const std::exception& ex) {
                LOG_ERROR("config.get: exception: %s", ex.what());
                return err_(rq, "config.get", -32010, ex.what());
            }
        }
    );

    reg.add(
        "config.save",
        "Save daemon configuration",
        [&](const RpcRequest& rq) -> RpcResult {
            const std::string pstr = paramsToString_(rq.params);
            LOG_DEBUG("rpc config.save params=%s", pstr.c_str());

            try {
                DaemonConfig current = defaultConfig();
                std::string cfgPath = current.configFile;
                if (!cfgPath.empty()) {
                    try { loadDaemonConfig(cfgPath, current); } catch (...) {}
                }

                DaemonConfig next = current;
                json in = rq.params.is_null() ? json::object() :
                          (rq.params.is_string() ? json::parse(rq.params.get<std::string>()) : rq.params);
                from_json(in, next);

                std::string savePath = !next.configFile.empty() ? next.configFile : cfgPath;
                if (savePath.empty()) {
                    savePath = defaultConfig().configFile;
                }

                saveDaemonConfig(savePath, next);

                self.setActiveProfileName(next.profileName);
                self.setEngineTickMs(next.tickMs);
                self.setEngineForceTickMs(next.forceTickMs);
                self.setEngineDeltaC(next.deltaC);

                return ok_(rq, "config.save", json{{"saved", true}, {"path", savePath}});
            } catch (const std::exception& ex) {
                LOG_WARN("config.save: invalid params or save error: %s", ex.what());
                return err_(rq, "config.save", -32602, ex.what());
            }
        }
    );

    reg.add(
        "config.set",
        "Set a single config key",
        [&](const RpcRequest& rq) -> RpcResult {
            const std::string pstr = paramsToString_(rq.params);
            LOG_DEBUG("rpc config.set params=%s", pstr.c_str());

            try {
                json in = rq.params.is_null() ? json::object() :
                          (rq.params.is_string() ? json::parse(rq.params.get<std::string>()) : rq.params);

                const std::string key = in.value("key", std::string{});
                if (key.empty() || !in.contains("value")) {
                    return err_(rq, "config.set", -32602, "missing key or value");
                }

                DaemonConfig cfg = defaultConfig();
                std::string cfgPath = cfg.configFile;
                if (!cfgPath.empty()) {
                    try { loadDaemonConfig(cfgPath, cfg); } catch (...) {}
                }

                const json& val = in["value"];
                if      (key == "profileName") cfg.profileName = val.get<std::string>();
                else if (key == "profilesDir") cfg.profilesDir = val.get<std::string>();
                else if (key == "tickMs")      cfg.tickMs      = val.get<int>();
                else if (key == "forceTickMs") cfg.forceTickMs = val.get<int>();
                else if (key == "deltaC")      cfg.deltaC      = val.get<double>();
                else if (key == "host")        cfg.host        = val.get<std::string>();
                else if (key == "port")        cfg.port        = val.get<int>();
                else if (key == "debug")       cfg.debug       = val.get<bool>();
                else if (key == "shmPath")     cfg.shmPath     = val.get<std::string>();
                else if (key == "pidfile" || key == "pidFile") cfg.pidfile = val.get<std::string>();
                else if (key == "logfile" || key == "logPath") cfg.logfile = val.get<std::string>();
                else if (key == "configFile")  cfg.configFile  = val.get<std::string>();
                else {
                    return err_(rq, "config.set", -32602, "unknown key");
                }

                std::string savePath = !cfg.configFile.empty() ? cfg.configFile :
                                       (!cfgPath.empty() ? cfgPath : defaultConfig().configFile);
                saveDaemonConfig(savePath, cfg);

                self.setActiveProfileName(cfg.profileName);
                self.setEngineTickMs(cfg.tickMs);
                self.setEngineForceTickMs(cfg.forceTickMs);
                self.setEngineDeltaC(cfg.deltaC);

                return ok_(rq, "config.set", json{{"saved", true}, {"path", savePath}});
            } catch (const std::exception& ex) {
                LOG_WARN("config.set: invalid params: %s", ex.what());
                return err_(rq, "config.set", -32602, ex.what());
            }
        }
    );
}

} // namespace lfc
