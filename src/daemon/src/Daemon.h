#pragma once
#include "Engine.h"
#include "Hwmon.h"
#include <string>
#include <vector>
#include <mutex>

/* Daemon:
 * - owns Engine
 * - holds in-memory channels
 * - handles simple JSON-RPC (very small parser/formatter)
 */

class Daemon {
public:
    bool init();
    void shutdown();
    void pump();

    std::string handleRequest(const std::string& jsonLine);

private:
    std::mutex mu_;
    Engine engine_;
    std::vector<TempSensor> temps_;
    std::vector<PwmDevice>  pwms_;
    std::vector<Channel>    chans_;
    int nextId_ = 1;

    // RPC helpers
    std::string rpc_enumerate();
    std::string rpc_listChannels();
    std::string rpc_createChannel(const std::string& name, const std::string& sensor, const std::string& pwm, const std::string& enable);
    std::string rpc_deleteChannel(const std::string& id);
    std::string rpc_setChannelMode(const std::string& id, const std::string& mode);
    std::string rpc_setChannelManual(const std::string& id, double pct);
    std::string rpc_setChannelCurve(const std::string& id, const std::vector<CurvePoint>& pts);
    std::string rpc_setChannelHystTau(const std::string& id, double hyst, double tau);
    std::string rpc_engineStart();
    std::string rpc_engineStop();
    std::string rpc_detectCoupling(double hold_s, double min_dC, int rpm_delta);

    // tiny helpers
    static std::string jsonEscape(const std::string& s);
    static std::string ok(const std::string& payload = "true");
    static std::string err(const std::string& msg);
    static bool findString(const std::string& body, const char* key, std::string& out);
    static bool findDouble(const std::string& body, const char* key, double& out);
};
