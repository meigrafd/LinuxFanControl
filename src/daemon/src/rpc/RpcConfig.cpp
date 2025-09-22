/*
 * Linux Fan Control — RPC: Configuration endpoints (full file)
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

void BindRpcConfig(Daemon& self, CommandRegistry& reg) {
    // ---------------------------------------------------------------------
    // config.get — Return effective daemon configuration from disk
    // ---------------------------------------------------------------------
    reg.add(
        "config.get",
        "Get effective daemon configuration",
        [&](const RpcRequest& rq) -> RpcResult {
            (void)rq;
            try {
                // Start from defaults and then overlay the persisted file (if present)
                DaemonConfig cfg = defaultConfig();

                std::string path = cfg.configFile;
                if (!path.empty()) {
                    try {
                        loadDaemonConfig(path, cfg);
                    } catch (const std::exception& ex) {
                        LOG_WARN("config.get: load failed from '%s': %s", path.c_str(), ex.what());
                        // Return defaults + error, and mark as not loaded
                        return ok_(rq, "config.get", json{
                            {"config",  cfg},
                            {"loaded",  false},
                            {"error",   ex.what()}
                        });
                    }
                }
                return ok_(rq, "config.get", json{
                    {"config", cfg},
                    {"loaded", true}
                });
            } catch (const std::exception& ex) {
                LOG_ERROR("config.get: exception: %s", ex.what());
                return err_(rq, "config.get", -32010, ex.what());
            }
        }
    );

    // ---------------------------------------------------------------------
    // config.save — Save entire daemon configuration object to disk
    // ---------------------------------------------------------------------
    reg.add(
        "config.save",
        "Save daemon configuration",
        [&](const RpcRequest& rq) -> RpcResult {
            const std::string pstr = paramsToString(rq.params);
            LOG_DEBUG("rpc config.save params=%s", pstr.c_str());

            try {
                // Load current config (defaults overlaid by file, when present)
                DaemonConfig current = defaultConfig();
                std::string cfgPath = current.configFile;
                if (!cfgPath.empty()) {
                    try { loadDaemonConfig(cfgPath, current); } catch (...) {}
                }

                // Merge incoming JSON onto the current config
                DaemonConfig next = current;
                json in = rq.params.is_null() ? json::object()
                                              : (rq.params.is_string()
                                                    ? json::parse(rq.params.get<std::string>())
                                                    : rq.params);
                from_json(in, next);

                // Resolve target save path (prefer explicit next.configFile)
                std::string savePath = !next.configFile.empty() ? next.configFile : cfgPath;
                if (savePath.empty()) {
                    savePath = defaultConfig().configFile;
                }

                // Persist to disk
                saveDaemonConfig(savePath, next);

                // Apply a few runtime-safe knobs
                self.setActiveProfileName(next.profileName);
                self.setEngineTickMs(next.tickMs);
                self.setEngineForceTickMs(next.forceTickMs);
                self.setEngineDeltaC(next.deltaC);

                return ok_(rq, "config.save", json{
                    {"saved", true},
                    {"path",  savePath}
                });
            } catch (const std::exception& ex) {
                LOG_WARN("config.save: invalid params or save error: %s", ex.what());
                return err_(rq, "config.save", -32602, ex.what());
            }
        }
    );

    // ---------------------------------------------------------------------
    // config.set — Set a single key (persisted), keep schema and behavior
    // ---------------------------------------------------------------------
    reg.add(
        "config.set",
        "Set a single config key",
        [&](const RpcRequest& rq) -> RpcResult {
            const std::string pstr = paramsToString(rq.params);
            LOG_DEBUG("rpc config.set params=%s", pstr.c_str());

            try {
                json in = rq.params.is_null() ? json::object()
                                              : (rq.params.is_string()
                                                    ? json::parse(rq.params.get<std::string>())
                                                    : rq.params);

                const std::string key = in.value("key", std::string{});
                if (key.empty() || !in.contains("value")) {
                    return err_(rq, "config.set", -32602, "missing key or value");
                }

                // Load current effective config
                DaemonConfig cfg = defaultConfig();
                std::string cfgPath = cfg.configFile;
                if (!cfgPath.empty()) {
                    try { loadDaemonConfig(cfgPath, cfg); } catch (...) {}
                }

                // Assign supported keys (minimal extension only, no refactor)
                const json& val = in["value"];
                if      (key == "profileName")          cfg.profileName  = val.get<std::string>();
                else if (key == "profilesPath")         cfg.profilesPath = val.get<std::string>();
                else if (key == "tickMs")               cfg.tickMs       = val.get<int>();
                else if (key == "forceTickMs")          cfg.forceTickMs  = val.get<int>();
                else if (key == "deltaC")               cfg.deltaC       = val.get<double>();
                else if (key == "host")                 cfg.host         = val.get<std::string>();
                else if (key == "port")                 cfg.port         = val.get<int>();
                else if (key == "debug")                cfg.debug        = val.get<bool>();
                else if (key == "shmPath")              cfg.shmPath      = val.get<std::string>();
                else if (key == "pidfile" || key == "pidFile")
                                                       cfg.pidfile       = val.get<std::string>();
                else if (key == "logfile" || key == "logPath")
                                                       cfg.logfile       = val.get<std::string>();
                else if (key == "configFile")           cfg.configFile   = val.get<std::string>();
                else if (key == "vendorMapPath")        cfg.vendorMapPath        = val.get<std::string>();
                else if (key == "vendorMapWatchMode")   cfg.vendorMapWatchMode   = val.get<std::string>();
                else if (key == "vendorMapThrottleMs")  cfg.vendorMapThrottleMs  = val.get<int>();
                else {
                    return err_(rq, "config.set", -32602, "unknown key");
                }

                // Save back to disk (prefer cfg.configFile if set by the call)
                std::string savePath = !cfg.configFile.empty() ? cfg.configFile
                                       : (!cfgPath.empty() ? cfgPath : defaultConfig().configFile);
                saveDaemonConfig(savePath, cfg);

                // Apply a few runtime-safe knobs similar to config.save
                if (key == "profileName") {
                    self.setActiveProfileName(cfg.profileName);
                } else if (key == "tickMs") {
                    self.setEngineTickMs(cfg.tickMs);
                } else if (key == "forceTickMs") {
                    self.setEngineForceTickMs(cfg.forceTickMs);
                } else if (key == "deltaC") {
                    self.setEngineDeltaC(cfg.deltaC);
                }

                return ok_(rq, "config.set", json{
                    {"saved", true},
                    {"path",  savePath},
                    {"key",   key}
                });
            } catch (const std::exception& ex) {
                LOG_WARN("config.set failed: %s", ex.what());
                return err_(rq, "config.set", -32602, ex.what());
            }
        }
    );
}

} // namespace lfc
