/*
 * Linux Fan Control — Command Registry (implementation)
 * - JSON-RPC method table, metadata, and dispatch
 * (c) 2025 LinuxFanControl contributors
 */
#include "include/CommandRegistry.h"

namespace lfc {

void CommandRegistry::registerMethod(const std::string& name, const std::string& help, RpcHandler fn) {
    handlers[name] = std::move(fn);
    helps[name] = help;
}

bool CommandRegistry::has(const std::string& name) const {
    return handlers.find(name) != handlers.end();
}

RpcResult CommandRegistry::call(const RpcRequest& req) const {
    auto it = handlers.find(req.method);
    if (it == handlers.end()) {
        return {false, "{\"code\":-32601,\"message\":\"method not found\"}"};
    }
    return it->second(req);
}

std::vector<CommandInfo> CommandRegistry::list() const {
    std::vector<CommandInfo> v;
    v.reserve(helps.size());
    for (const auto& kv : helps) v.push_back(CommandInfo{kv.first, kv.second});
    return v;
}

} // namespace lfc
