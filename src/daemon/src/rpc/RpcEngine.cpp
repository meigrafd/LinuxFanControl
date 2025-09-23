/*
 * Linux Fan Control — RPC: Engine API (enable/disable/reset/status)
 * (c) 2025 LinuxFanControl contributors
 */

#include <nlohmann/json.hpp>
#include "include/CommandRegistry.hpp"  // ok_ / err_
#include "include/Daemon.hpp"
#include "include/Engine.hpp"
#include "include/Profile.hpp"
#include "include/Log.hpp"

namespace lfc {

using nlohmann::json;

// Forward-declare; not needed here but keeps binder signature uniform.
class Daemon;

void BindRpcEngine(Daemon& self, CommandRegistry& reg) {
    // engine.enable — turn control engine on
    reg.add(
        "engine.enable",
        "Enable the control engine",
        [&self](const RpcRequest& rq) -> RpcResult {
            LOG_TRACE("rpc engine.enable");
            self.engineEnable(true);
            return ok_(rq, "engine.enable", json{{"enabled", true}});
        }
    );

    // engine.disable — turn control engine off (fans revert to system/last state)
    reg.add(
        "engine.disable",
        "Disable the control engine",
        [&self](const RpcRequest& rq) -> RpcResult {
            LOG_TRACE("rpc engine.disable");
            self.engineEnable(false);
            return ok_(rq, "engine.disable", json{{"enabled", false}});
        }
    );

    // engine.reset — clear internal engine state (does not change enabled flag)
    reg.add(
        "engine.reset",
        "Reset engine internal state",
        [&self](const RpcRequest& rq) -> RpcResult {
            LOG_TRACE("rpc engine.reset");
            self.engineEnable(false);
            self.applyProfile(Profile{});
            return ok_(rq, "engine.reset", json{{"reset", true}, {"enabled", self.engineControlEnabled()}});
        }
    );

    // engine.status — report current engine state
    reg.add(
        "engine.status",
        "Return current engine state",
        [&self](const RpcRequest& rq) -> RpcResult {
            const bool en = self.engineControlEnabled();
            const int  tk = self.engineTickMs();
            const int  fk = self.engineForceTickMs();
            const double dc = self.engineDeltaC();
            LOG_DEBUG("RPC engine.status enabled=%d tickMs=%d forceTickMs=%d deltaC=%.3f",
                    en ? 1 : 0, tk, fk, dc);
            json j{{"enabled", en},{"tickMs", tk},{"forceTickMs", fk},{"deltaC", dc}};
            return ok_(rq, "engine.status", j);
        }
    );
}

} // namespace lfc
