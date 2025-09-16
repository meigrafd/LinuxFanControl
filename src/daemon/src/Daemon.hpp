/*
 * Linux Fan Control — Daemon (header)
 * - Orchestrates hwmon scan, engine, detection, RPC, and telemetry
 * - Matches Engine API (enable(), enabled(), telemetryJson(), attachTelemetry(), setHwmonView())
 * - init takes DaemonConfig& (non-const) to allow default/bootstrap adjustments
 * (c) 2025 LinuxFanControl contributors
 */
#pragma once

#include <atomic>
#include <memory>
#include <string>
#include <vector>

#include "Config.hpp"         // provides DaemonConfig (cfg_ is by value)
#include "Hwmon.hpp"          // HwmonSnapshot by value
#include "Engine.hpp"         // Engine API
#include "ShmTelemetry.hpp"   // SHM writer
#include "RpcTcpServer.hpp"   // RPC server
#include "include/CommandRegistry.h"
#include "Profile.hpp"        // Profile apply/load

namespace lfc {

// Forward decl only for pointer member
class Detection;

class Daemon {
public:
    Daemon();
    ~Daemon();

    // Bootstrap and lifecycle
    bool init(DaemonConfig& cfg, bool debugCli, const std::string& cfgPath, bool foreground);
    void runLoop();
    void shutdown();
    void requestStop() { running_.store(false, std::memory_order_relaxed); }

    // Config file path and active config snapshot
    const std::string& configPath() const { return configPath_; }
    const DaemonConfig& cfg() const { return cfg_; }
    void setCfg(const DaemonConfig& c) { cfg_ = c; }

    // Telemetry and engine control
    bool telemetryGet(std::string& out) const;
    bool engineControlEnabled() const;
    void engineEnable(bool on);

    // Engine gating knobs (applied in runLoop)
    double engineDeltaC() const { return deltaC_; }
    void setEngineDeltaC(double v);
    int engineForceTickMs() const { return forceTickMs_; }
    void setEngineForceTickMs(int v);
    int engineTickMs() const { return tickMs_; }
    void setEngineTickMs(int v);

    // Profile handling
    bool applyProfileFile(const std::string& path);
    bool applyProfile(const Profile& p);
    Profile currentProfile() const;

    // Detection worker
    bool detectionStart();
    void detectionAbort();
    struct DetectionStatus {
        bool running{false};
        int currentIndex{0};
        int total{0};
        std::string phase;
    };
    DetectionStatus detectionStatus() const;
    std::vector<int> detectionResults() const;

    // RPC helpers
    std::vector<CommandInfo> listRpcCommands() const;

    // Accessors used by RPC handlers
    Engine& engine() { return *engine_; }
    const Engine& engine() const { return *engine_; }
    HwmonSnapshot& hwmon() { return hwmon_; }
    const HwmonSnapshot& hwmon() const { return hwmon_; }

private:
    // Internal helpers (declared here; defined in Daemon.cpp)
    static std::string ensure_profile_filename(const std::string& name);
    static int getenv_int(const char* key, int def);
    static double getenv_double(const char* key, double def);
    bool applyProfileIfValid(const std::string& profilePath);

private:
    // Runtime configuration (by value -> needs Config.hpp)
    DaemonConfig cfg_{};
    std::string configPath_;
    bool debug_{false};
    bool foreground_{false};

    // Engine + telemetry
    std::unique_ptr<Engine> engine_;
    ShmTelemetry telemetry_;

    // Discovered hwmon devices
    HwmonSnapshot hwmon_;

    // Detection worker
    std::unique_ptr<Detection> detection_;
    std::vector<int> detectionPeakRpm_;

    // RPC
    std::unique_ptr<CommandRegistry> rpcRegistry_;
    std::unique_ptr<RpcTcpServer> rpcServer_;

    // Main loop state
    std::atomic<bool> running_{false};
    std::atomic<bool> stop_{false};

    // Gating/timing (overridable by env LFCD_* and config)
    int tickMs_{25};          // LFCD_TICK_MS (5..1000)
    double deltaC_{0.5};      // LFCD_DELTA_C  (0.0..10.0)
    int forceTickMs_{1000};   // LFCD_FORCE_TICK_MS (100..10000)
};

} // namespace lfc
