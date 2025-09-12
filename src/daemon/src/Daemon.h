#pragma once
// Daemon interface (JSON-RPC 2.0 with batch).
// Adds: detectCalibrate (port of Python auto-setup: perturbation + calibration).
// Also enforces single-instance via Unix socket probing.
// Comments in English per project guideline.

#include <string>
#include <atomic>
#include <nlohmann/json.hpp>

class Hwmon;
class Engine;

class Daemon {
public:
    Daemon();
    ~Daemon();

    bool init();                  // bind/listen (returns false if another instance is running)
    bool pumpOnce(int timeoutMs); // accept one client with timeout; true => keep looping
    void shutdown();              // stop server

private:
    // RPC plumbing
    nlohmann::json dispatch(const nlohmann::json& req);

    // Core RPCs
    nlohmann::json rpcEnumerate();
    nlohmann::json rpcListChannels();

    bool rpcCreateChannel(const std::string& name,
                          const std::string& sensor,
                          const std::string& pwm);
    bool rpcDeleteChannel(const std::string& id);
    bool rpcSetChannelMode(const std::string& id, const std::string& mode);
    bool rpcSetChannelManual(const std::string& id, double pct);
    bool rpcSetChannelCurve(const std::string& id,
                            const std::vector<std::pair<double,double>>& pts);
    bool rpcSetChannelHystTau(const std::string& id, double hyst, double tau);
    bool rpcDeleteCoupling(const std::string& id);

    // Auto-setup (detection + calibration), returns JSON summary.
    nlohmann::json rpcDetectCalibrate();

    static nlohmann::json error_obj(const nlohmann::json& id, int code, const std::string& msg);
    static nlohmann::json result_obj(const nlohmann::json& id, const nlohmann::json& result);

    // single-instance helpers
    bool isAlreadyRunning() const; // try connect to existing socket

private:
    std::string  sockPath_;
    int          srvFd_;
    std::atomic<bool> running_;
    bool         debug_; // controlled by env LFC_DEBUG

    // owned components
    Hwmon*  hw_;
    Engine* engine_;
};
