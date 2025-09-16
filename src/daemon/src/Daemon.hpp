/*
 * Linux Fan Control — Daemon (header)
 * - Lifecycle, RPC, engine, detection
 * (c) 2025 LinuxFanControl contributors
 */
#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "Config.hpp"
#include "Engine.hpp"
#include "Hwmon.hpp"
#include "include/CommandRegistry.h"
#include "RpcTcpServer.hpp"
#include "Detection.hpp"

namespace lfc {

class Daemon {
public:
    Daemon();
    ~Daemon();

    bool init(DaemonConfig& cfg, bool debugCli, const std::string& cfgPath, bool foreground);
    void runLoop();
    void shutdown();
    void pumpOnce(int timeoutMs = 0);

    // RPC entry point used by RpcTcpServer
    RpcResult dispatch(const std::string& method, const std::string& paramsJson);
    std::vector<CommandInfo> listRpcCommands() const;

    // Accessors for handlers
    const std::string& configPath() const { return configPath_; }
    DaemonConfig& cfg() { return cfg_; }
    const DaemonConfig& cfg() const { return cfg_; }
    void setCfg(const DaemonConfig& c) { cfg_ = c; }

    HwmonSnapshot& hwmon() { return hwmon_; }
    const HwmonSnapshot& hwmon() const { return hwmon_; }
    Engine& engine() { return engine_; }
    bool telemetryGet(std::string& out) { return engine_.getTelemetry(out); }
    bool engineControlEnabled() const { return engine_.controlEnabled(); }

    bool applyProfileFile(const std::string& path) { return applyProfileIfValid(path); }
    void engineEnable(bool on) { engine_.enableControl(on); }

    // detection proxies (thread-safe)
    bool detectionStart();
    void detectionAbort();
    Detection::Status detectionStatus() const;
    std::vector<int> detectionResults() const;

    // loop tuning accessors
    double engineDeltaC() const { return deltaC_; }
    int engineForceTickMs() const { return forceTickMs_; }
    void setEngineDeltaC(double v) { if (v >= 0.0 && v <= 10.0) deltaC_ = v; }
    void setEngineForceTickMs(int v) { if (v >= 100 && v <= 10000) forceTickMs_ = v; }

    // lifecycle
    void requestStop() { running_.store(false); }

    // friend to let RpcHandlers access private members if required later
    friend void BindDaemonRpcCommands(Daemon& self, CommandRegistry& reg);

private:
    static bool ensureDir(const std::string& path);
    bool writePidFile(const std::string& path);
    void removePidFile();
    bool applyProfileIfValid(const std::string& profilePath);

private:
    std::atomic<bool> running_{false};
    std::string pidFile_;
    std::string configPath_;
    DaemonConfig cfg_{};

    HwmonSnapshot hwmon_{};
    Engine engine_{};

    std::unique_ptr<CommandRegistry> reg_;
    std::unique_ptr<RpcTcpServer> rpc_;

    // detection
    mutable std::mutex detMu_;
    std::unique_ptr<Detection> detection_;

    // gating knobs
    double deltaC_{0.5};
    int forceTickMs_{1000};
};

} // namespace lfc
