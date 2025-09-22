/*
 * Linux Fan Control — Daemon (header)
 * - Orchestrates engine, hwmon scan, GPU monitor, telemetry and RPC
 * (c) 2025 LinuxFanControl contributors
 */
#pragma once

#include <atomic>
#include <memory>
#include <string>
#include <vector>
#include <thread>
#include <mutex>

#include "Config.hpp"
#include "Profile.hpp"
#include "Hwmon.hpp"
#include "Engine.hpp"
#include "GpuMonitor.hpp"
#include "ShmTelemetry.hpp"
#include "RpcTcpServer.hpp"
#include "include/CommandRegistry.hpp"
#include "Detection.hpp"

namespace lfc {

class Detection;

class Daemon {
public:
    Daemon();
    explicit Daemon(std::string cfgPath);
    ~Daemon();

    bool init(const DaemonConfig& cfg, bool debugCli = false);
    bool init(); // no-arg wrapper: uses current cfg_/debug_
    void runLoop();
    void requestStop() { stop_.store(true, std::memory_order_relaxed); }
    void shutdown();

    // Startup/config setters used by main & RPC
    const std::string& configPath() const noexcept { return configPath_; }
    void setConfigPath(const std::string& p) { configPath_ = p; }

    inline bool enabled() const noexcept { return enabled_.load(std::memory_order_relaxed); }
    inline void setEnabled(bool v) noexcept { enabled_.store(v, std::memory_order_relaxed); }

    // Engine convenience API (used by RPC)
    bool   engineControlEnabled() const noexcept { return enabled(); }
    int    engineTickMs()        const noexcept { return cfg_.tickMs; }
    int    engineForceTickMs()   const noexcept { return cfg_.forceTickMs; }
    double engineDeltaC()        const noexcept { return cfg_.deltaC; }
    void   engineEnable(bool on) noexcept { setEnabled(on); }
    void   setEngineTickMs(int ms) noexcept { if (ms > 0) cfg_.tickMs = ms; }
    void   setEngineForceTickMs(int ms) noexcept { if (ms > 0) cfg_.forceTickMs = ms; }
    void   setEngineDeltaC(double dc) noexcept { if (dc >= 0.0) cfg_.deltaC = dc; }

    // Runtime config setters/getters (programmatic)
    void setProfilesPath(const std::string& d) { cfg_.profilesPath = d; }
    void setActiveProfile(const std::string& n) { setActiveProfileName(n); }
    void setRpcHost(const std::string& h) { cfg_.host = h; }
    void setRpcPort(int p) { cfg_.port = p; }
    void setShmPath(const std::string& p) { cfg_.shmPath = p; }
    void setDebug(bool on) { debug_ = on; cfg_.debug = on; }

    // Config access
    const DaemonConfig& config() const noexcept { return cfg_; }

    Profile&       profile()       noexcept { return profile_; }
    const Profile& profile() const noexcept { return profile_; }

    HwmonSnapshot&       hwmon()       noexcept { return hwmon_; }
    const HwmonSnapshot& hwmon() const noexcept { return hwmon_; }

    std::vector<GpuSample>&       gpus()       noexcept { return gpus_; }
    const std::vector<GpuSample>& gpus() const noexcept { return gpus_; }

    bool telemetryGet(std::string& outJson) const;

    CommandRegistry& rpcRegistry() noexcept { return *rpcRegistry_; }
    RpcTcpServer&    rpcServer()   noexcept { return *rpcServer_;  }

    std::string profilesPath() const;
    std::string profilePathForName(const std::string& name) const;
    std::string activeProfileName() const { return cfg_.profileName; }
    void        setActiveProfileName(const std::string& n) { cfg_.profileName = n; }
    void        applyProfile(const Profile& p);

    // Detection control
    bool detectionStart();
    bool detectionStatus(DetectResult& out) const;
    void detectionRequestStop();
    void detectionAbort() { detectionRequestStop(); }

    // Restart control
    void requestRestart() { restart_.store(true, std::memory_order_relaxed); }
    bool restartRequested() const noexcept { return restart_.load(std::memory_order_relaxed); }

private:
    void refreshHwmon();
    void refreshGpus();
    void publishTelemetry();

    void rememberOriginalEnables();
    void restoreOriginalEnables();

private:
    std::string   configPath_;
    DaemonConfig  cfg_{};
    std::atomic<bool> enabled_{false};
    std::atomic<bool> stop_{false};
    std::atomic<bool> running_{false};
    bool debug_{false};
    std::atomic<bool> restart_{false};

    HwmonSnapshot                          hwmon_;
    std::vector<GpuSample>                 gpus_;
    Profile                                profile_;
    std::unique_ptr<ShmTelemetry>          telemetry_;
    std::unique_ptr<Engine>                engine_;
    std::unique_ptr<RpcTcpServer>          rpcServer_;
    std::unique_ptr<CommandRegistry>       rpcRegistry_;
    std::unique_ptr<Detection>             detection_;

    std::vector<std::pair<std::string,int>> origPwmEnable_;

    mutable std::mutex                      detectMtx_;
    std::thread                             detectThread_;
    std::atomic<bool>                       detectRunning_{false};
    DetectResult                            detectResult_;
};

// inline no-arg init wrapper
inline bool Daemon::init() { return init(cfg_, debug_); }

} // namespace lfc
