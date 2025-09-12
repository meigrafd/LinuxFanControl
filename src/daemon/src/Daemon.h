#pragma once
// Daemon interface expected by src/daemon/src/main.cpp
// - init(): bind/listen on Unix domain socket (single-instance guard)
// - pumpOnce(timeout_ms): accept+serve exactly one client (or timeout)
// - shutdown(): close & unlink socket
// Comments in English per project preference.

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
