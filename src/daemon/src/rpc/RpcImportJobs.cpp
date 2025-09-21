/*
 * Linux Fan Control — RPC: profile.importJobs
 * (c) 2025 LinuxFanControl contributors
 */
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

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

void BindRpcImportJobs(Daemon& self, CommandRegistry& reg) {
    (void)self;

    reg.add(
        "profile.importJobs",
        "List all active import jobs",
        [&](const RpcRequest& rq) -> RpcResult {
            (void)rq;
            LOG_TRACE("rpc profile.importJobs");

            std::vector<ImportStatus> list = ImportJobManager::instance().list();
            json arr = json::array();
            for (const auto& st : list) {
                arr.push_back(json{
                    {"jobId", st.jobId},
                    {"state", st.state},
                    {"progress", st.progress},
                    {"message", st.message},
                    {"error", st.error},
                    {"profileName", st.profileName},
                    {"isFanControlRelease", st.isFanControlRelease}
                });
            }

            LOG_DEBUG("profile.importJobs: count=%zu", arr.size());
            return ok_("profile.importJobs", arr);
        });
}

} // namespace lfc
