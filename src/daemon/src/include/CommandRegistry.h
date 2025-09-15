/*
 * Linux Fan Control — Command Registry (header)
 * - RPC command table and helpers
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
        using Handler = std::function<RpcResult(const RpcRequest&)>;

        void add(const std::string& name, const std::string& help, Handler fn) {
            handlers_[name] = std::move(fn);
            help_[name] = help;
        }

        RpcResult call(const RpcRequest& req) const {
            auto it = handlers_.find(req.method);
            if (it == handlers_.end()) return {false, "{\"error\":{\"code\":-32601,\"message\":\"method not found\"}}"};
            return it->second(req);
    }

    std::vector<CommandInfo> list() const {
        std::vector<CommandInfo> v;
        v.reserve(help_.size());
        for (auto& kv : help_) v.push_back(CommandInfo{kv.first, kv.second});
        return v;
    }

    const std::unordered_map<std::string, Handler>& handlers() const { return handlers_; }
    const std::unordered_map<std::string, std::string>& help() const { return help_; }

private:
    std::unordered_map<std::string, Handler> handlers_;
    std::unordered_map<std::string, std::string> help_;
};

} // namespace lfc
