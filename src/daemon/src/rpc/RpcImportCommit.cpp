/*
 * Linux Fan Control — RPC: profile.importCommit
 * (c) 2025 LinuxFanControl contributors
 */
#include <nlohmann/json.hpp>
#include <string>

#include "include/Daemon.hpp"
#include "include/CommandRegistry.hpp"
#include "include/Profile.hpp"
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

static inline json params_to_json(const json& in) {
    if (in.is_null()) return json::object();
    if (in.is_string()) {
        try {
            const auto& s = in.get_ref<const std::string&>();
            json p = json::parse(s, nullptr, false);
            if (p.is_discarded()) return json::object();
            return p;
        } catch (...) {
            return json::object();
        }
    }
    if (in.is_object()) return in;
    return json::object();
}

void BindRpcImportCommit(Daemon& self, CommandRegistry& reg) {
    reg.add(
        "profile.importCommit",
        "Commit a finished import job: save profile and set active",
        [&self](const RpcRequest& rq) -> RpcResult {
            LOG_TRACE("rpc profile.importCommit params=%s", rq.params.dump().c_str());

            json p = params_to_json(rq.params);
            if (!p.contains("jobId") || !p["jobId"].is_string()) {
                LOG_WARN("profile.importCommit: missing 'jobId'");
                return err_("profile.importCommit", -32602, "missing 'jobId'");
            }

            const std::string jobId = p["jobId"].get<std::string>();
            std::string err;

            const bool ok = ImportJobManager::instance().commit(
                jobId,
                // Save-and-activate callback
                [&self](const Profile& prof, std::string& errOut) -> bool {
                    try {
                        const std::string path = self.profilePathForName(prof.name);
                        saveProfileToFile(prof, path);
                        self.setActiveProfileName(prof.name);
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
                    return err_("profile.importCommit", -32031, err);
                }
                return err_("profile.importCommit", -32033, err.empty() ? "commit failed" : err);
            }

            return ok_("profile.importCommit", json{{"ok", true}});
        }
    );
}

} // namespace lfc
