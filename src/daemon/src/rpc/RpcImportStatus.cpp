/*
 * Linux Fan Control — RPC: profile.importStatus
 * (c) 2025 LinuxFanControl contributors
 */
#include <nlohmann/json.hpp>
#include <string>

#include "include/Daemon.hpp"
#include "include/CommandRegistry.hpp"
#include "rpc/RpcImportAPI.hpp"
#include "rpc/ImportJobs.hpp"
#include "include/Log.hpp"

namespace lfc {

using nlohmann::json;

static inline RpcResult ok_(const char* m, const json& d=json::object()){
    json o{{"method",m},{"success",true},{"data",d}};
    return {true,o.dump()};
}
static inline RpcResult err_(const char* m, int c, const std::string& msg){
    json o{{"method",m},{"success",false},{"error", {{"code",c},{"message",msg}}}};
    return {false,o.dump()};
}

static inline json params_to_json(const json& in)
{
    if (in.is_null()) {
        return json::object();
    }
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
    if (in.is_object()) {
        return in;
    }
    return json::object();
}

static bool rpc_profile_importStatus_(const json& params, json& result, std::string& errOut){
    (void)errOut;

    if (!params.is_object() ||
        !params.contains("jobId") || !params.at("jobId").is_string()) {
        result = json{{"error","missing jobId"}};
        LOG_WARN("profile.importStatus: missing or invalid 'jobId' param");
        return false;
    }

    const std::string id = params.value("jobId", "");
    bool found = true;
    LOG_TRACE("profile.importStatus: lookup jobId='%s'", id.c_str());

    ImportStatus st = ImportJobManager::instance().get(id, &found);
    if (!found) {
        result = json{{"error","job not found"}};
        LOG_WARN("profile.importStatus: job not found (jobId='%s')", id.c_str());
        return false;
    }

    result = json{
        {"jobId", st.jobId},
        {"state", st.state},
        {"progress", st.progress},
        {"message", st.message},
        {"error", st.error},
        {"profileName", st.profileName},
        {"isFanControlRelease", st.isFanControlRelease}
    };

    LOG_DEBUG("profile.importStatus: jobId='%s' state=%s progress=%d",
              st.jobId.c_str(), st.state.c_str(), st.progress);
    return true;
}

void BindRpcImportStatus(Daemon& self, CommandRegistry& reg){
    (void)self;

    reg.add("profile.importStatus","Get async import status",[&](const RpcRequest& rq)->RpcResult{
        LOG_TRACE("rpc profile.importStatus params=%s", rq.params.dump().c_str());
        json p = params_to_json(rq.params);

        json res; std::string e;
        bool ok = rpc_profile_importStatus_(p,res,e);
        if(!ok && !e.empty()) {
            LOG_WARN("rpc profile.importStatus: error=%s", e.c_str());
            return err_("profile.importStatus",-32602,e);
        }
        if(!ok) {
            const std::string msg = res.value("error","invalid params");
            LOG_WARN("profile.importStatus: %s", msg.c_str());
            return err_("profile.importStatus",-32602,msg);
        }

        return ok_("profile.importStatus",res);
    });
}

} // namespace lfc
