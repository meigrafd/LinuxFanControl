/*
 * Linux Fan Control — RPC: profile.importAs
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

static inline json params_to_json(const json& in)
{
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

static bool rpc_profile_importAs_(const json& params, json& result, std::string& errOut)
{
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

void BindRpcImportAs(Daemon& self, CommandRegistry& reg)
{
    (void)self;

    reg.add(
        "profile.importAs",
        "Start async import of a profile; returns {jobId}",
        [&](const RpcRequest& rq) -> RpcResult {
            json p = params_to_json(rq.params);

            json out; std::string err;
            if (!rpc_profile_importAs_(p, out, err)) {
                const std::string msg = out.value("error", err.empty() ? "invalid params" : err);
                LOG_ERROR("profile.importAs: %s", msg.c_str());
                return err_("profile.importAs", -32602, msg);
            }
            return ok_("profile.importAs", out);
        });
}

} // namespace lfc
