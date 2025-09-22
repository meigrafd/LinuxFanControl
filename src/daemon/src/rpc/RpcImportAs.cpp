/*
 * Linux Fan Control — RPC: profile.importAs
 * (c) 2025 LinuxFanControl contributors
 */

#include <nlohmann/json.hpp>
#include <string>

#include "include/Daemon.hpp"
#include "include/CommandRegistry.hpp"   // uses central ok_ / err_
#include "rpc/ImportJobs.hpp"
#include "include/Log.hpp"

namespace lfc {

using nlohmann::json;

// Internal implementation; returns true on success and fills 'result' or 'errOut'.
static bool rpc_profile_importAs_(const json& params, json& result, std::string& errOut) {
    (void)errOut;

    LOG_TRACE("profile.importAs: params=%s", params.dump().c_str());

    if (!params.is_object() ||
        !params.contains("path") || !params.at("path").is_string() ||
        !params.contains("name") || !params.at("name").is_string())
    {
        result = json{{"error", "missing 'path' or 'name'"}};
        LOG_WARN("profile.importAs: missing 'path' or 'name'");
        return false;
    }

    const std::string path = params.at("path").get<std::string>();
    const std::string name = params.at("name").get<std::string>();
    const bool validateDetect = params.value("validateDetect", false);
    const int  rpmMin         = params.value("rpmMin", 300);
    const int  timeoutMs      = params.value("timeoutMs", 60000);

    const std::string jobId = ImportJobManager::instance().create(
        path, name, validateDetect, rpmMin, timeoutMs);

    if (jobId.empty()) {
        result = json{{"error", "failed to start import job"}};
        LOG_ERROR("profile.importAs: failed to start job (path='%s', name='%s')", path.c_str(), name.c_str());
        return false;
    }

    result = json{{"jobId", jobId}};
    LOG_INFO("profile.importAs: started jobId='%s'", jobId.c_str());
    return true;
}

void BindRpcImportAs(Daemon& self, CommandRegistry& reg) {
    (void)self;

    reg.add(
        "profile.importAs",
        "Start async import of a profile; returns {jobId}",
        [&](const RpcRequest& rq) -> RpcResult {
            const json p = paramsToJson(rq);

            json out; std::string err;
            if (!rpc_profile_importAs_(p, out, err)) {
                const std::string msg = out.value("error", err.empty() ? "invalid params" : err);
                LOG_ERROR("profile.importAs: %s", msg.c_str());
                return err_(rq, "profile.importAs", -32602, msg);
            }
            return ok_(rq, "profile.importAs", out);
        }
    );
}

} // namespace lfc
