/*
 * Linux Fan Control — Daemon (header)
 * - Orchestrates engine, hwmon, detection and RPC server
 * (c) 2025 LinuxFanControl contributors
 */
#pragma once

#include <atomic>
#include <memory>
#include <string>
#include <vector>

#include "Config.hpp"       // DaemonConfig
#include "Profile.hpp"
#include "Hwmon.hpp"
#include "ShmTelemetry.hpp"

namespace lfc {

class Engine;
class RpcTcpServer;
class Detection;

struct CommandInfo; // from include/CommandRegistry.h

struct DetectionStatus {
    bool        running{false};
    int         currentIndex{0};
    int         total{0};
    std::string phase;
};

class Daemon {
public:
    Daemon();
    ~Daemon();

    bool init(DaemonConfig& cfg, bool debugCli, const std::string& cfgPath, bool foreground);
    void runLoop();
    void shutdown();
    void requestStop() { stop_.store(true, std::memory_order_relaxed); }

    // telemetry / engine
    bool telemetryGet(std::string& out) const;
    bool engineControlEnabled() const;
    void engineEnable(bool on);
    void setEngineDeltaC(double v);
    void setEngineForceTickMs(int v);
    void setEngineTickMs(int v);
    double engineDeltaC() const { return deltaC_; }
    int    engineForceTickMs() const { return forceTickMs_; }
    int    engineTickMs() const { return tickMs_; }

    // detection
    bool              detectionStart();
    void              detectionAbort();
    DetectionStatus   detectionStatus() const;
    std::vector<int>  detectionResults() const;

    // profiles
    bool    applyProfile(const struct Profile& p);
    bool    applyProfileFile(const std::string& path);
    bool    applyProfileIfValid(const std::string& path);
    struct Profile currentProfile() const;

    // rpc registry
    std::vector<CommandInfo> listRpcCommands() const;

    // cfg / paths
    const HwmonInventory& hwmon() const { return hwmon_; }
    const std::string&    configPath() const { return configPath_; }
    void                  setCfg(const DaemonConfig& c) { cfg_ = c; }
    const DaemonConfig&   cfg() const { return cfg_; }

private:
    static std::string ensure_profile_filename(const std::string& name);
    static int         getenv_int(const char* key, int def);
    static double      getenv_double(const char* key, double def);

private:
    DaemonConfig   cfg_;
    bool           debug_{false};
    bool           foreground_{false};
    std::string    configPath_;

    std::atomic<bool> running_{false};
    std::atomic<bool> stop_{false};

    ShmTelemetry     telemetry_;
    HwmonInventory   hwmon_;

    std::unique_ptr<Engine>               engine_;
    std::unique_ptr<RpcTcpServer>         rpcServer_;
    std::unique_ptr<class CommandRegistry> rpcRegistry_;
    std::unique_ptr<Detection>            detection_;

    int     tickMs_{25};
    int     forceTickMs_{1000};
    double  deltaC_{0.5};

    std::vector<std::pair<std::string,int>> origPwmEnable_;
};

} // namespace lfc
