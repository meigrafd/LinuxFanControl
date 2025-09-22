/*
 * Linux Fan Control — RPC: profile.importStatus
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

void BindRpcImportStatus(Daemon& self, CommandRegistry& reg) {
    (void)self;

    reg.add(
        "profile.importStatus",
        "Return import job status",
        [&](const RpcRequest& rq) -> RpcResult {
            LOG_TRACE("rpc profile.importStatus params=%s", rq.params.dump().c_str());

            const json p = paramsToJson(rq);
            if (!p.contains("jobId") || !p.at("jobId").is_string()) {
                return err_(rq, "profile.importStatus", -32602, "missing jobId");
            }

            const std::string id = p.at("jobId").get<std::string>();
            bool found = true;
            const ImportStatus st = ImportJobManager::instance().get(id, &found); // ← per ImportJobs.hpp
            if (!found) {
                return err_(rq, "profile.importStatus", -32031, "job not found");
            }

            json out{
                {"jobId",               st.jobId},
                {"state",               st.state},
                {"progress",            st.progress},
                {"message",             st.message},
                {"error",               st.error},
                {"profileName",         st.profileName},
                {"isFanControlRelease", st.isFanControlRelease}
            };

            return ok_(rq, "profile.importStatus", out);
        }
    );
}

} // namespace lfc
