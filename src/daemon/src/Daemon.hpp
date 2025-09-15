#pragma once
/*
 * Linux Fan Control — Daemon
 * JSON-RPC over TCP (no HTTP), SHM telemetry, detection, channels, logging.
 * (c) 2025 LinuxFanControl contributors
 */

#include <string>
#include <memory>
#include <atomic>
#include "include/CommandRegistry.h"
#include "Engine.hpp"
#include "Config.hpp"

namespace lfc {

  class RpcTcpServer;

  class Daemon {
  public:
    Daemon();
    ~Daemon();

    bool init(const Config& cfg);
    void runLoop();
    void pumpOnce(int timeoutMs=200);
    void shutdown();

    // PID
    bool writePidFile(const std::string& path);
    void removePidFile();

    // command reg
    void bindCommands(CommandRegistry& reg);

  private:
    std::unique_ptr<RpcTcpServer> rpc_;
    std::unique_ptr<CommandRegistry> reg_;
    std::string pidFile_;
    std::atomic<bool> running_{false};
    int rpcTimeoutMs_{200};

    // subsystems
    Engine engine_;
    HwmonSnapshot hwmon_;

    // helpers
    std::string jsonError(const char* msg) const;
  };

} // namespace lfc
