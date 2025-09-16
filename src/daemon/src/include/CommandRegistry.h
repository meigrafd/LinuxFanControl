/*
 * Linux Fan Control — Command Registry (header)
 * - JSON-RPC method table, metadata, and dispatch declarations
 * (c) 2025 LinuxFanControl contributors
 */
#pragma once

#include <functional>
#include <map>
#include <string>
#include <vector>

namespace lfc {

struct CommandInfo {
    std::string name;
    std::string help;
};

struct RpcRequest {
    std::string method;
    std::string id;
    std::string params;     // unified: RpcTcpServer + Daemon use "params"
};

struct RpcResult {
    bool ok{true};
    std::string json;
};

class CommandRegistry {
public:
    using RpcHandler = std::function<RpcResult(const RpcRequest&)>;

    void registerMethod(const std::string& name, const std::string& help, RpcHandler fn);
    bool has(const std::string& name) const;
    RpcResult call(const RpcRequest& req) const;
    std::vector<CommandInfo> list() const;

private:
    std::map<std::string, RpcHandler> handlers;
    std::map<std::string, std::string> helps;
};

} // namespace lfc
