/*
 * Linux Fan Control — RPC: Core bindings (commands/help/ping/version)
 * (c) 2025 LinuxFanControl contributors
 */

#include <nlohmann/json.hpp>
#include <string>

#include "include/CommandRegistry.hpp"      // provides ok_ / err_
#include "include/CommandIntrospection.hpp"
#include "include/Version.hpp"
#include "include/Log.hpp"

namespace lfc {

using nlohmann::json;

// Forward-declare; not needed here but keeps binder signature uniform.
class Daemon;

static inline std::string param_name(const json& params) {
    if (params.is_object() && params.contains("name") && params["name"].is_string())
        return params["name"].get<std::string>();
    if (params.is_string()) {
        try {
            json p = json::parse(params.get_ref<const std::string&>());
            if (p.is_object() && p.contains("name") && p["name"].is_string())
                return p["name"].get<std::string>();
        } catch (...) {}
    }
    return {};
}

void BindRpcCore(Daemon& /*self*/, CommandRegistry& reg) {
    reg.add(
        "commands",
        "List available RPC commands",
        [&reg](const RpcRequest& rq) -> RpcResult {
            LOG_TRACE("rpc commands");
            return ok_(rq, "commands", reg.listJson());
        }
    );

    // Show help for a command: { "name": "<command>" }
    reg.add(
        "help",
        "Show help for a command",
        [&reg](const RpcRequest& rq) -> RpcResult {
            LOG_TRACE("rpc help");
            const std::string name = param_name(rq.params);
            if (name.empty())
                return err_(rq, "help", -32602, "missing 'name'");
            auto h = reg.help(name);
            if (!h)
                return err_(rq, "help", -32601, "unknown command");
            return ok_(rq, "help", json{{"name", name}, {"help", *h}});
        }
    );

    // Liveness probe
    reg.add(
        "ping",
        "Liveness probe",
        [](const RpcRequest& rq) -> RpcResult {
            LOG_TRACE("rpc ping");
            return ok_(rq, "ping", json{{"pong", true}});
        }
    );

    // Daemon/RPC version info
    reg.add(
        "version",
        "Return daemon/rpc version info",
        [](const RpcRequest& rq) -> RpcResult {
            LOG_TRACE("rpc version");
            json data{
                {"name",    "LinuxFanControl"},
                {"version", LFCD_VERSION},
                {"rpc",     "2.0"}
            };
            return ok_(rq, "version", data);
        }
    );

}

} // namespace lfc
