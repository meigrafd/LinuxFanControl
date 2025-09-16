/*
 * Linux Fan Control — Daemon (header)
 * - JSON-RPC (TCP) server + SHM telemetry
 * - PID file handling (/run primary, /tmp fallback persisted)
 * - Config/profile bootstrap and import mapping
 * - Non-blocking detection worker
 * (c) 2025 LinuxFanControl contributors
 */
#pragma once
#include <string>
#include <vector>
#include <memory>
#include <atomic>
#include <mutex>
#include "Config.hpp"
#include "Engine.hpp"
#include "Detection.hpp"
#include "include/CommandRegistry.h"

namespace lfc {

class RpcTcpServer;

class Daemon {
public:
    Daemon();
    ~Daemon();

    bool init(DaemonConfig& cfg, bool debugCli, const std::string& cfgPath);
    void runLoop();
    void pumpOnce(int timeoutMs = 200);
    void shutdown();

    bool writePidFile(const std::string& path);
    void removePidFile();

    void bindCommands(CommandRegistry& reg);

    RpcResult dispatch(const std::string& method, const std::string& paramsJson);
    std::vector<CommandInfo> listRpcCommands() const;

private:
    bool applyProfileIfValid(const std::string& profilePath);

private:
    std::unique_ptr<RpcTcpServer> rpc_;
    std::unique_ptr<CommandRegistry> reg_;
    std::string pidFile_;
    std::atomic<bool> running_{false};
    int rpcTimeoutMs_{200};

    std::string configPath_;
    DaemonConfig cfg_;

    Engine engine_;
    HwmonSnapshot hwmon_;

    std::mutex detMu_;
    std::unique_ptr<Detection> detection_;
};

} // namespace lfc
