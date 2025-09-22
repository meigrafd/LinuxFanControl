/*
 * Linux Fan Control — RPC: telemetry.json
 * (c) 2025 LinuxFanControl contributors
 */

#include <nlohmann/json.hpp>
#include <string>

#include "include/CommandRegistry.hpp"   // ok_ / err_
#include "include/Daemon.hpp"
#include "include/Log.hpp"

namespace lfc {

using nlohmann::json;

void BindRpcTelemetry(Daemon& self, CommandRegistry& reg) {
    reg.add("telemetry.json", "Return current SHM JSON blob", [&](const RpcRequest& rq) -> RpcResult {
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
        return ok_(rq, "telemetry.json", j);
    });
}


} // namespace lfc
