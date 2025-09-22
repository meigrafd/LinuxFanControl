/*
 * Linux Fan Control — RPC: profile.importJobs
 * (c) 2025 LinuxFanControl contributors
 */
#include <nlohmann/json.hpp>

#include "include/Daemon.hpp"
#include "include/CommandRegistry.hpp"
#include "include/Log.hpp"
#include "rpc/ImportJobs.hpp"

namespace lfc {

using nlohmann::json;

void BindRpcImportJobs(Daemon& self, CommandRegistry& reg) {
    (void)self;

    reg.add(
        "profile.importJobs",
        "List profile import jobs",
        [&](const RpcRequest& rq) -> RpcResult {
            try {
                LOG_DEBUG("rpc profile.importJobs params=%s", rq.params.dump().c_str());

                const auto jobsVec = ImportJobManager::instance().list();

                json jobs = json::array();
                for (const auto& s : jobsVec) {
                    jobs.push_back(json(s)); // relies on NLOHMANN_DEFINE_TYPE_INTRUSIVE in ImportStatus
                }

                return ok_(rq, "profile.importJobs", json{{"jobs", jobs}});
            } catch (const std::exception& ex) {
                LOG_ERROR("rpc profile.importJobs failed: %s", ex.what());
                return err_(rq, "profile.importJobs", -32603, ex.what());
            } catch (...) {
                LOG_ERROR("rpc profile.importJobs failed: unknown error");
                return err_(rq, "profile.importJobs", -32603, "internal error");
            }
        }
    );
}

} // namespace lfc
