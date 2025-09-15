#include "include/CommandRegistry.h"
using namespace lfc;

void CommandRegistry::registerMethod(const std::string& name, const std::string& help, RpcHandler fn) {
    map_[name] = {help, std::move(fn)};
}
bool CommandRegistry::has(const std::string& name) const { return map_.find(name) != map_.end(); }
RpcResult CommandRegistry::call(const RpcRequest& req) const {
    auto it = map_.find(req.method);
    if (it == map_.end()) return {false, std::string("{\"error\":{\"code\":-32601,\"message\":\"method not found: ")+req.method+"\"}}"};
    return it->second.second(req);
}
std::vector<CommandInfo> CommandRegistry::list() const {
    std::vector<CommandInfo> v; v.reserve(map_.size());
    for (auto& kv : map_) v.push_back({kv.first, kv.second.first});
    return v;
}
