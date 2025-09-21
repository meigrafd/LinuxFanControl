/*
 * Linux Fan Control — RPC: Hwmon listing
 * Release build — full file
 * (c) 2025 LinuxFanControl contributors
 */

#include <nlohmann/json.hpp>
#include <string>

#include "include/Daemon.hpp"
#include "include/Hwmon.hpp"
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

static inline RpcResult err_(const RpcRequest& rq, const char* method, const std::string& message, const json& data = json::object()) {
    (void)method;
    return RpcResult::makeError(
        rq.id, -1, message, data
    );
}

/* -------------------------------- bindings -------------------------------- */
void BindRpcHwmonList(Daemon& /*self*/, CommandRegistry& reg) {
    // list.sensor — list temperature inputs
    reg.add(
        "list.sensor",
        "List temperature inputs",
        [&](const RpcRequest& rq) -> RpcResult {
            LOG_TRACE("rpc list.sensor");
            auto snap = Hwmon::scan();
            json arr = json::array();
            for (const auto& t : snap.temps) {
                arr.push_back(json{
                    // NOTE: HwmonTemp fields: chipPath, path_input, label
                    {"chip",   t.chipPath},
                    {"input",  t.path_input},
                    {"label",  t.label},
                    {"name",   t.label}  // keep key for clients; use label as name
                });
            }
            return ok_(rq, "list.sensor", arr);
        }
    );

    // list.fan — list tach inputs (RPM)
    reg.add(
        "list.fan",
        "List tach inputs (RPM)",
        [&](const RpcRequest& rq) -> RpcResult {
            LOG_TRACE("rpc list.fan");
            auto snap = Hwmon::scan();
            json arr = json::array();
            for (const auto& f : snap.fans) {
                arr.push_back(json{
                    // HwmonFan fields: chipPath, path_input, label
                    {"chip",   f.chipPath},
                    {"input",  f.path_input},
                    {"label",  f.label},
                    {"name",   f.label}
                });
            }
            return ok_(rq, "list.fan", arr);
        }
    );

    // list.pwm — list PWM controls
    reg.add(
        "list.pwm",
        "List PWM controls",
        [&](const RpcRequest& rq) -> RpcResult {
            LOG_TRACE("rpc list.pwm");
            auto snap = Hwmon::scan();
            json arr = json::array();
            for (const auto& p : snap.pwms) {
                arr.push_back(json{
                    // HwmonPwm fields: chipPath, path_pwm, path_enable, pwm_max, label
                    {"chip",      p.chipPath},
                    {"pwm",       p.path_pwm},
                    {"enable",    p.path_enable},
                    {"label",     p.label},
                    {"name",      p.label},
                    {"hasEnable", !p.path_enable.empty()}
                });
            }
            return ok_(rq, "list.pwm", arr);
        }
    );
}

} // namespace lfc
