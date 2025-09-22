/*
 * Linux Fan Control — RPC: profile.importCancel
 * (c) 2025 LinuxFanControl contributors
 */

#include <nlohmann/json.hpp>
#include <string>

#include "include/Daemon.hpp"
#include "include/CommandRegistry.hpp"   // ok_ / err_
#include "rpc/ImportJobs.hpp"
#include "include/Log.hpp"

namespace lfc {

using nlohmann::json;

void BindRpcImportCancel(Daemon& self, CommandRegistry& reg) {
    (void)self;

    reg.add(
        "profile.importCancel",
        "Cancel import job",
        [&](const RpcRequest& rq) -> RpcResult {
            LOG_TRACE("rpc profile.importCancel params=%s", rq.params.dump().c_str());

            const json p = paramsToJson(rq);
            if (!p.contains("jobId") || !p.at("jobId").is_string()) {
                return err_(rq, "profile.importCancel", -32602, "missing jobId");
            }

            const std::string jobId = p.at("jobId").get<std::string>();
            const bool ok = ImportJobManager::instance().cancel(jobId);
            if (!ok) {
                // Domain-specific error for non-cancelable or unknown jobs
                return err_(rq, "profile.importCancel", -32032, "cancel failed or not cancelable");
            }

            return ok_(rq, "profile.importCancel", json{{"jobId", jobId}, {"canceled", true}});
        }
    );
}

} // namespace lfc
