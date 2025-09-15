#pragma once
#include <string>
#include <vector>
#include <memory>
#include <atomic>
#include "Config.hpp"
#include "Engine.hpp"

namespace lfc {

  class CommandRegistry;
  class RpcTcpServer;

  struct CommandInfo {
    std::string name;
    std::string help;
  };

  struct RpcRequest {
    std::string method;
    std::string id;
    std::string paramsJson;
  };

  struct RpcResult {
    bool ok{false};
    std::string json;
  };

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
