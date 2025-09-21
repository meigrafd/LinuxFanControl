/*
 * Linux Fan Control — RPC: profile.importCancel
 * (c) 2025 LinuxFanControl contributors
 */
#include <nlohmann/json.hpp>
#include <string>

#include "include/Daemon.hpp"
#include "include/CommandRegistry.hpp"
#include "rpc/ImportJobs.hpp"
#include "include/Log.hpp"

namespace lfc {

using nlohmann::json;

static inline RpcResult ok_(const char* m, const json& d = json::object()) {
    json o{{"method", m}, {"success", true}, {"data", d}};
    return {true, o.dump()};
}
static inline RpcResult err_(const char* m, int c, const std::string& msg) {
    json o{{"method", m}, {"success", false}, {"error", {{"code", c}, {"message", msg}}}};
    return {false, o.dump()};
}

void BindRpcImportCancel(Daemon& self, CommandRegistry& reg) {
    (void)self;

    reg.add(
        "profile.importCancel",
        "Cancel an import job",
        [&](const RpcRequest& rq) -> RpcResult {
            LOG_TRACE("rpc profile.importCancel params=%s", rq.params.dump().c_str());

            // Expect: { "jobId": "<id>" }
            if (!rq.params.is_object() || !rq.params.contains("jobId") || !rq.params["jobId"].is_string()) {
                return err_("profile.importCancel", -32602, "missing 'jobId'");
            }
            const std::string jobId = rq.params["jobId"].get<std::string>();

            bool found = false;
            const bool ok = ImportJobManager::instance().cancel(jobId, &found);
            if (!found) {
                return err_("profile.importCancel", -32031, "job not found");
            }
            if (!ok) {
                return err_("profile.importCancel", -32032, "job not cancelable");
            }

            LOG_INFO("profile.importCancel: canceled jobId='%s'", jobId.c_str());
            return ok_("profile.importCancel", json{{"jobId", jobId}});
        });
}

} // namespace lfc
