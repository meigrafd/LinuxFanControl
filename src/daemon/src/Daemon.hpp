/*
 * Linux Fan Control — Daemon (header)
 * - JSON-RPC (TCP) server + SHM telemetry
 * - PID file + file logging
 * - Config bootstrap and command registry
 * (c) 2025 LinuxFanControl contributors
 */
#pragma once
#include <string>
#include <vector>
#include <memory>
#include <atomic>
#include "Config.hpp"
#include "Engine.hpp"
#include "include/CommandRegistry.h"

namespace lfc {

  class RpcTcpServer;

  class Daemon {
  public:
    Daemon();
    ~Daemon();

    bool init(const DaemonConfig& cfg, bool debugCli);
    void runLoop();
    void pumpOnce(int timeoutMs = 200);
    void shutdown();

    bool writePidFile(const std::string& path);
    void removePidFile();

    void bindCommands(CommandRegistry& reg);

    std::string dispatch(const std::string& method, const std::string& paramsJson);
    std::vector<CommandInfo> listRpcCommands() const;

  private:
    std::unique_ptr<RpcTcpServer> rpc_;
    std::unique_ptr<CommandRegistry> reg_;
    std::string pidFile_;
    std::atomic<bool> running_{false};
    int rpcTimeoutMs_{200};

    Engine engine_;
    HwmonSnapshot hwmon_;
  };

} // namespace lfc
