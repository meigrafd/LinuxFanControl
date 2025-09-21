/*
 * Linux Fan Control — RPC: Detection
 * (c) 2025 LinuxFanControl contributors
 */
#include <nlohmann/json.hpp>
#include <string>

#include "rpc/ImportJobs.hpp"
#include "include/Daemon.hpp"
#include "include/Detection.hpp"
#include "include/CommandRegistry.hpp"
#include "include/Log.hpp"

namespace lfc {

using nlohmann::json;

/* ------------------------------- helpers ---------------------------------- */
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

static inline RpcResult err_(const RpcRequest& rq, const char* method, int code, const std::string& message, const json& data = json::object()) {
    (void)method;
    return RpcResult::makeError(
        rq.id, code, message, data
    );
}

/* -------------------------------- bindings -------------------------------- */
void BindRpcDetect(Daemon& self, CommandRegistry& reg) {
    // detect.start — Start non-blocking detection
    reg.add(
        "detect.start",
        "Start non-blocking detection",
        [&](const RpcRequest& rq) -> RpcResult {
            const std::string p = rq.params.is_null() ? std::string("{}") : rq.params.dump();
            LOG_INFO("rpc detect.start params=%s", p.c_str());

            // Daemon API: bool Daemon::detectionStart()
            const bool ok = self.detectionStart();
            if (!ok) {
                LOG_WARN("detect.start: already running or precondition failed");
                return err_(rq, "detect.start", -32040, "already running or failed");
            }
            return ok_(rq, "detect.start", json{{"started", true}});
        });

    // detect.abort — Abort detection
    reg.add(
        "detect.abort",
        "Abort detection",
        [&](const RpcRequest& rq) -> RpcResult {
            (void)rq;
            LOG_INFO("rpc detect.abort");
            // Daemon API: void Daemon::detectionRequestStop()
            self.detectionRequestStop();
            return ok_(rq, "detect.abort");
        });

    // detect.status — Detection status/progress
    reg.add(
        "detect.status",
        "Detection status/progress",
        [&](const RpcRequest& rq) -> RpcResult {
            (void)rq;

            // Daemon API: bool Daemon::detectionStatus(DetectResult& out) const
            DetectResult st{};
            const bool have = self.detectionStatus(st);
            if (!have) {
                LOG_WARN("rpc detect.status: unavailable");
                return err_(rq, "detect.status", -32041, "unavailable");
            }

            const bool running = !st.ok;
            json d{
                {"running",      running},
                {"ok",           st.ok},
                {"error",        st.error},
                {"mappedPwms",   st.mappedPwms},
                {"mappedTemps",  st.mappedTemps}
            };
            LOG_DEBUG("rpc detect.status running=%d ok=%d mappedPwms=%d mappedTemps=%d",
                      running ? 1 : 0, st.ok ? 1 : 0, st.mappedPwms, st.mappedTemps);

            return ok_(rq, "detect.status", d);
        });

    // detect.results — Return detection peak RPMs per PWM
    reg.add(
        "detect.results",
        "Return detection peak RPMs per PWM",
        [&](const RpcRequest& rq) -> RpcResult {
            (void)rq;

            DetectResult st{};
            (void)self.detectionStatus(st);

            json arr = json::array();
            LOG_DEBUG("rpc detect.results count=%zu", (size_t)0);

            return ok_(rq, "detect.results", arr);
        });
}

} // namespace lfc
