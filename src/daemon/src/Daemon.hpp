/*
 * Linux Fan Control — Daemon (header)
 * Drives initialization, control loop, RPC and graceful shutdown.
 * NOTE: DaemonConfig is defined in Config.hpp — do NOT redefine here.
 * (c) 2025 LinuxFanControl contributors
 */
#pragma once

#include "Config.hpp"
#include "Hwmon.hpp"
#include "ShmTelemetry.hpp"
#include "Profile.hpp"

#include <atomic>
#include <memory>
#include <string>
#include <vector>
#include <utility>

namespace lfc {

// Forward declarations to keep this header light
class Engine;
class RpcTcpServer;
class CommandRegistry;
struct CommandInfo;
class Detection;
struct DetectionStatus;   // <-- definition lives in Detection.hpp

class Daemon {
public:
    Daemon();
    ~Daemon();

    // lifecycle
    bool init(DaemonConfig& cfg, bool debugCli, const std::string& cfgPath, bool foreground);
    void runLoop();
    void shutdown();
    inline void requestStop() { stop_.store(true, std::memory_order_relaxed); }

    // telemetry / engine
    bool  telemetryGet(std::string& out) const;
    bool  engineControlEnabled() const;
    void  engineEnable(bool on);

    void  setEngineDeltaC(double v);
    void  setEngineForceTickMs(int v);
    void  setEngineTickMs(int v);

    // getters used by RPC layer
    inline double engineDeltaC()      const { return deltaC_; }
    inline int    engineForceTickMs() const { return forceTickMs_; }
    inline int    engineTickMs()      const { return tickMs_; }
    Engine&                   engine()            { return *engine_; }
    const Engine&             engine()      const { return *engine_; }
    const HwmonInventory&     hwmon()       const { return hwmon_; }

    // detection
    bool               detectionStart();
    void               detectionAbort();
    DetectionStatus    detectionStatus() const;
    std::vector<int>   detectionResults() const;

    // profiles
    bool   applyProfile(const Profile& p);
    bool   applyProfileFile(const std::string& path);
    bool   applyProfileIfValid(const std::string& path);
    Profile currentProfile() const;

    // RPC registry helpers
    std::vector<CommandInfo> listRpcCommands() const;

    // config accessors (used by RpcHandlers)
    inline const DaemonConfig& cfg() const { return cfg_; }
    inline void setCfg(const DaemonConfig& c) { cfg_ = c; }
    inline const std::string& configPath() const { return configPath_; }
    inline bool foreground() const { return foreground_; }

private:
    // helpers
    static int         getenv_int(const char* key, int def);
    static double      getenv_double(const char* key, double def);
    static std::string ensure_profile_filename(const std::string& name);

private:
    // runtime config/state
    DaemonConfig  cfg_;
    bool          debug_{false};
    bool          foreground_{false};
    std::string   configPath_;

    std::atomic<bool> running_{false};
    std::atomic<bool> stop_{false};

    // subsystems
    ShmTelemetry                     telemetry_;
    HwmonInventory                   hwmon_;
    std::unique_ptr<Engine>          engine_;
    std::unique_ptr<RpcTcpServer>    rpcServer_;
    std::unique_ptr<CommandRegistry> rpcRegistry_;
    std::unique_ptr<Detection>       detection_;

    // loop tuning
    int     tickMs_{25};
    int     forceTickMs_{1000};
    double  deltaC_{0.5};

    // original pwm*_enable states (path_enable, value) — restored on shutdown
    std::vector<std::pair<std::string,int>> origPwmEnable_;
};

} // namespace lfc
