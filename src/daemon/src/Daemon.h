#pragma once
// Orchestrates Hwmon scan and RPC server

#include <string>
#include <vector>
#include <mutex>
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

    // RPC handlers
    std::string onCommand(const std::string& line);
    std::string jsonListTemps() const;
    std::string jsonListPwms()  const;

    // Helpers
    static std::string jsonEscape(const std::string& s);
};
