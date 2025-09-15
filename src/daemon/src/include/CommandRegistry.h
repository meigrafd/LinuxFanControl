/*
 * Linux Fan Control — Command Registry (header)
 * - RPC command table, metadata, and dispatch
 * (c) 2025 LinuxFanControl contributors
 */
#pragma once
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace lfc {

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
        std::unordered_map<std::string, RpcHandler> map_;
        std::unordered_map<std::string, std::string> help_;
    };

} // namespace lfc
