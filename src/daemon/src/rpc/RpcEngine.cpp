/*
 * Linux Fan Control — RPC engine handlers
 * (c) 2025 LinuxFanControl contributors
 */
#include "include/Engine.hpp"
#include "include/Daemon.hpp"
#include "include/CommandRegistry.hpp"
#include "include/Log.hpp"
#include <nlohmann/json.hpp>

namespace lfc {

using nlohmann::json;

static inline RpcResult ok(const char* m, const json& d = json::object()) {
    json out{{"method",m},{"success",true},{"data",d}}; return {true,out.dump()};
}

void BindRpcEngine(Daemon& self, CommandRegistry& reg) {
    reg.add("engine.enable", "Enable automatic control", [&](const RpcRequest&) -> RpcResult {
        LOG_INFO("RPC engine.enable");
        self.engineEnable(true);
        return ok("engine.enable");
    });
    reg.add("engine.disable", "Disable automatic control", [&](const RpcRequest&) -> RpcResult {
        LOG_INFO("RPC engine.disable");
        self.engineEnable(false);
        return ok("engine.disable");
    });
    reg.add("engine.reset", "Disable control and clear profile", [&](const RpcRequest&) -> RpcResult {
        LOG_INFO("RPC engine.reset");
        self.engineEnable(false);
        self.applyProfile(Profile{});
        return ok("engine.reset");
    });
    reg.add("engine.status", "Basic engine status", [&](const RpcRequest&) -> RpcResult {
        const bool en = self.engineControlEnabled();
        const int  tk = self.engineTickMs();
        const int  fk = self.engineForceTickMs();
        const double dc = self.engineDeltaC();
        LOG_DEBUG("RPC engine.status enabled=%d tickMs=%d forceTickMs=%d deltaC=%.3f",
                  en ? 1 : 0, tk, fk, dc);
        json j{{"enabled",en},{"tickMs",tk},{"forceTickMs",fk},{"deltaC",dc}};
        return ok("engine.status", j);
    });
}

} // namespace lfc
