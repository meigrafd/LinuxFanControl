/*
 * Linux Fan Control — Command Registry (header-only)
 * - JSON-RPC method table, metadata, and dispatch
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

    void registerMethod(const std::string& name, const std::string& help, RpcHandler fn) {
        map_[name] = std::move(fn);
        help_[name] = help;
    }

    bool has(const std::string& name) const {
        return map_.find(name) != map_.end();
    }

    RpcResult call(const RpcRequest& req) const {
        auto it = map_.find(req.method);
        if (it == map_.end()) {
            return {false, "{\"code\":-32601,\"message\":\"method not found\"}"};
        }
        return it->second(req);
    }

    std::vector<CommandInfo> list() const {
        std::vector<CommandInfo> v;
        v.reserve(help_.size());
        for (const auto& kv : help_) v.push_back(CommandInfo{kv.first, kv.second});
        return v;
    }

private:
    std::unordered_map<std::string, RpcHandler> map_;
    std::unordered_map<std::string, std::string> help_;
};

} // namespace lfc
