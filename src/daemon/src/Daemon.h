#pragma once
// Daemon: orchestrates hwmon scan and exposes a JSON-RPC 2.0 (batch-only) API over a Unix domain socket.
// Comments in English per project preference.

#include <string>
#include <vector>
#include <mutex>
#include <optional>
#include <nlohmann/json.hpp>
#include "Engine.h"
#include "Hwmon.h"
#include "RpcServer.h"

class Daemon {
public:
    bool init();
    void shutdown();
    void pumpOnce(int pollMs);

private:
    Hwmon hw_;
    RpcServer rpc_;
    std::mutex mtx_; // protects cached lists
    std::vector<TempSensorInfo> temps_;
    std::vector<PwmOutputInfo>  pwms_;
    Engine engine_;

    // Socket entrypoint: expects a single line containing a JSON array (batch).
    std::string onCommand(const std::string& jsonLine);

    // JSON-RPC
    std::string handleJsonRpcBatch(const std::string& jsonText);   // returns JSON text (+ '\n')
    nlohmann::json jsonRpcCall(const nlohmann::json& req);         // single request -> response or null (notification)

    // Legacy JSON builders (used to compose results quickly)
    std::string jsonListTemps() const;
    std::string jsonListPwms()  const;
    std::string jsonEnumerate() const;
    std::string jsonListChannels() const;

    // Helpers
    static std::string jsonEscape(const std::string& s);
    static std::vector<std::pair<double,double>> parsePoints(const std::string& payload);
};
