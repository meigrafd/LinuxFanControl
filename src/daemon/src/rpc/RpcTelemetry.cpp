/*
 * Linux Fan Control — RPC telemetry handlers
 * (c) 2025 LinuxFanControl contributors
 */
#include "include/Daemon.hpp"
#include "include/CommandRegistry.hpp"
#include "include/Log.hpp"
#include <nlohmann/json.hpp>
#include <string>

namespace lfc {

using nlohmann::json;

static inline RpcResult ok(const char* m, const json& d = json::object()) {
    json out{{"method",m},{"success",true},{"data",d}}; return {true,out.dump()};
}

void BindRpcTelemetry(Daemon& self, CommandRegistry& reg) {
    reg.add("telemetry.json", "Return current SHM JSON blob", [&](const RpcRequest&) -> RpcResult {
        std::string blob;
        if (!self.telemetryGet(blob)) {
            LOG_WARN("telemetry.json: no SHM data, returning empty object");
            blob = "{}";
        } else {
            LOG_DEBUG("telemetry.json: got SHM blob size=%zu", blob.size());
        }
        json j = json::parse(blob, nullptr, false);
        if (j.is_discarded()) {
            LOG_WARN("telemetry.json: parse failed, returning empty object");
            j = json::object();
        }
        return ok("telemetry.json", j);
    });
}

} // namespace lfc
