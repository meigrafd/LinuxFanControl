/*
 * Linux Fan Control — Command Registry (implementation)
 * - Backing store and simple lookup
 * (c) 2025 LinuxFanControl contributors
 */
#include "include/CommandRegistry.h"

namespace lfc {

void CommandRegistry::registerMethod(const std::string& name, const std::string& help, RpcHandler fn) {
    map_[name] = std::move(fn);
    help_[name] = help;
}

bool CommandRegistry::has(const std::string& name) const {
    return map_.find(name) != map_.end();
}

RpcResult CommandRegistry::call(const RpcRequest& req) const {
    auto it = map_.find(req.method);
    if (it == map_.end()) return {false, "{\"error\":{\"code\":-32601,\"message\":\"method not found\"}}"};
    return it->second(req);
}

std::vector<CommandInfo> CommandRegistry::list() const {
    std::vector<CommandInfo> v;
    v.reserve(help_.size());
    for (const auto& kv : help_) v.push_back(CommandInfo{kv.first, kv.second});
    return v;
}

} // namespace lfc
