/*
 * Linux Fan Control — RPC: Core bindings (commands/help/ping/version)
 * (c) 2025 LinuxFanControl contributors
 */

#include <nlohmann/json.hpp>
#include <string>

#include "include/CommandRegistry.hpp"
#include "include/CommandIntrospection.hpp"
#include "include/Version.hpp"
#include "include/Log.hpp"

namespace lfc {

using nlohmann::json;

// Vorwärtsdeklaration reicht hier; der Parameter wird nicht verwendet.
class Daemon;

/*
 * BindRpcCore
 * Registriert Kernbefehle am CommandRegistry.
 * Signaturen sind auf RpcRequest/RpcResult angepasst.
 */
void BindRpcCore(Daemon& /*self*/, CommandRegistry& reg) {
    // rpc.commands — Liste aller Befehle
    reg.add(
        "commands",
        "List available RPC commands",
        [&reg](const RpcRequest& rq) -> RpcResult {
            LOG_TRACE("rpc commands");
            return RpcResult::makeOk(rq.id, reg.listJson());
        }
    );

    // rpc.help — Hilfe zu einem Befehl: { "name": "<command>" }
    reg.add(
        "help",
        "Show help for a command",
        [&reg](const RpcRequest& rq) -> RpcResult {
            LOG_TRACE("rpc help");
            const std::string name = rq.params.value("name", std::string{});
            if (name.empty()) {
                return RpcResult::makeError(rq.id, -32602, "missing 'name'");
            }
            auto h = reg.help(name);
            if (!h) {
                return RpcResult::makeError(rq.id, -32601, "unknown command", {{"name", name}});
            }
            return RpcResult::makeOk(rq.id, json{{"name", name}, {"help", *h}});
        }
    );

    // rpc.ping — Liveness-Probe
    reg.add(
        "ping",
        "Liveness probe",
        [](const RpcRequest& rq) -> RpcResult {
            LOG_TRACE("rpc ping");
            return RpcResult::makeOk(rq.id, json{{"pong", true}});
        }
    );

    // rpc.version — Versionsinfo (ohne Abhängigkeit auf bestimmte Makros)
    reg.add(
        "version",
        "Return daemon/rpc version info",
        [](const RpcRequest& rq) -> RpcResult {
            LOG_TRACE("rpc version");
            json data{
                {"name",    "LinuxFanControl"},
                {"version",  LFCD_VERSION},
                {"rpc",     "2.0"}
            };
            return RpcResult::makeOk(rq.id, data);
        }
    );
}

} // namespace lfc
