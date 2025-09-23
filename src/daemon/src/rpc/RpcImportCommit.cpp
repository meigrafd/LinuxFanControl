/*
 * Linux Fan Control — RPC: profile.importCommit
 * (c) 2025 LinuxFanControl contributors
 */

#include <nlohmann/json.hpp>
#include <string>

#include "include/Daemon.hpp"
#include "include/CommandRegistry.hpp"   // ok_ / err_
#include "include/Profile.hpp"
#include "rpc/ImportJobs.hpp"
#include "include/Log.hpp"

namespace lfc {

using nlohmann::json;

void BindRpcImportCommit(Daemon& self, CommandRegistry& reg) {
    reg.add(
        "profile.importCommit",
        "Commit a finished import job: save profile and set active",
        [&self](const RpcRequest& rq) -> RpcResult {
            LOG_TRACE("rpc profile.importCommit params=%s", rq.params.dump().c_str());

            const json p = paramsToJson(rq);
            if (!p.contains("jobId") || !p.at("jobId").is_string()) {
                LOG_WARN("profile.importCommit: missing 'jobId'");
                return err_(rq, "profile.importCommit", -32602, "missing 'jobId'");
            }

            const std::string jobId = p.at("jobId").get<std::string>();
            std::string err;

            // Commit via manager: we provide a save/apply callback.
            const bool ok = ImportJobManager::instance().commit(
                jobId,
                // Save-and-activate callback: persist the profile and set it active.
                [&self](const Profile& prof, std::string& errOut) -> bool {
                    try {
                        const std::string path = self.profilePathForName(prof.name);
                        saveProfileToFile(prof, path);
                        self.setActiveProfileName(prof.name);
                        // load the active profile into runtime so SHM reflects it
                        self.applyProfile(prof);
                        LOG_INFO("profile.importCommit: saved '%s' and set active", prof.name.c_str());
                        return true;
                    } catch (const std::exception& ex) {
                        errOut = ex.what();
                        LOG_ERROR("profile.importCommit: save/apply failed: %s", ex.what());
                        return false;
                    }
                },
                err
            );

            if (!ok) {
                if (err == "job not found") {
                    return err_(rq, "profile.importCommit", -32031, err);
                }
                return err_(rq, "profile.importCommit", -32033, err.empty() ? "commit failed" : err);
            }

            // Keep response schema consistent with the project's RPC API.
            return ok_(rq, "profile.importCommit", json{{"jobId", jobId}, {"committed", true}});
        }
    );
}

} // namespace lfc
