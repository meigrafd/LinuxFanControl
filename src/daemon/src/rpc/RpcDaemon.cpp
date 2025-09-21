/*
 * Linux Fan Control — RPC: Daemon control
 * (c) 2025 LinuxFanControl contributors
 */
#include <nlohmann/json.hpp>
#include <string>

#include "include/Daemon.hpp"
#include "include/UpdateChecker.hpp"
#include "include/CommandRegistry.hpp"
#include "include/Log.hpp"

namespace lfc {

using nlohmann::json;

static inline RpcResult ok_(const RpcRequest& rq, const char* method, const json& data = json::object()) {
    return RpcResult::makeOk(
        rq.id,
        json{
            {"method",  method},
            {"success", true},
            {"data",    data}
        }
    );
}

static inline RpcResult err_(const RpcRequest& rq, const char* method, const std::string& message, const json& data = json::object()) {
    (void)method; // method context is implicit in the message for now
    return RpcResult::makeError(
        rq.id,
        -1,          // generic error code (kept compatible with existing clients)
        message,
        data
    );
}

void BindRpcDaemon(Daemon& self, CommandRegistry& reg) {
    // daemon.shutdown — graceful stop
    reg.add(
        "daemon.shutdown",
        "Shutdown daemon gracefully",
        [&self](const RpcRequest& rq) -> RpcResult {
            LOG_INFO("rpc daemon.shutdown");
            self.requestStop();
            return ok_(rq, "daemon.shutdown", json{{"status", "stopping"}});
        }
    );

    // daemon.restart — request restart (handled by external supervisor if any)
    reg.add(
        "daemon.restart",
        "Request daemon restart",
        [&self](const RpcRequest& rq) -> RpcResult {
            LOG_INFO("rpc daemon.restart");
            self.requestRestart();
            return ok_(rq, "daemon.restart", json{{"status", "restarting"}});
        }
    );

    // daemon.update — check/download latest GitHub release
    // Params: { download?: bool, target?: string, repo?: "owner/name" }
    reg.add(
        "daemon.update",
        "Check/download latest release",
        [&self](const RpcRequest& rq) -> RpcResult {
            (void)self;

            const bool download      = rq.params.value("download", false);
            const std::string repo   = rq.params.value("repo",   std::string{"meigrafd/LinuxFanControl"});
            const std::string target = rq.params.value("target", std::string{});

            ReleaseInfo info;
            std::string fetchErr;

            const auto slash = repo.find('/');
            const std::string owner = (slash == std::string::npos) ? "meigrafd" : repo.substr(0, slash);
            const std::string name  = (slash == std::string::npos) ? "LinuxFanControl" : repo.substr(slash + 1);

            if (!UpdateChecker::fetchLatest(owner, name, info, fetchErr)) {
                LOG_WARN("[update] fetch failed: %s", fetchErr.c_str());
                return err_(rq, "daemon.update", fetchErr);
            }

            if (!download) {
                return ok_(rq, "daemon.update", json{
                    {"tag",    info.tag},
                    {"name",   info.name},
                    {"url",    info.htmlUrl},
                    {"assets", info.assets.size()}
                });
            }

            if (target.empty()) {
                const std::string m = "missing 'target' for download";
                LOG_WARN("[update] %s", m.c_str());
                return err_(rq, "daemon.update", m);
            }
            if (info.assets.empty()) {
                const std::string m = "no assets in latest release";
                LOG_WARN("[update] %s", m.c_str());
                return err_(rq, "daemon.update", m);
            }

            const std::string aurl = info.assets[0].url;
            std::string dlErr;
            if (!UpdateChecker::downloadToFile(aurl, target, dlErr)) {
                LOG_ERROR("[update] download failed: %s", dlErr.c_str());
                return err_(rq, "daemon.update", dlErr);
            }

            return ok_(rq, "daemon.update", json{
                {"downloaded", true},
                {"target",     target},
                {"tag",        info.tag},
                {"name",       info.name}
            });
        }
    );
}

} // namespace lfc
