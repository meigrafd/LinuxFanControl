/*
 * Linux Fan Control — RPC: Profile management
 * (c) 2025 LinuxFanControl contributors
 */
#include <nlohmann/json.hpp>
#include <string>
#include <filesystem>
#include <vector>
#include <system_error>

#include "include/Daemon.hpp"
#include "include/Profile.hpp"
#include "include/CommandRegistry.hpp"
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

// Historically used by the UI; keep a stub to avoid mismatches.
static inline std::string to_profile_name(const std::string& s) { return s; }

void BindRpcProfile(Daemon& self, CommandRegistry& reg) {
    // profile.getActive -> {"name": "<active-name>"}
    reg.add(
        "profile.getActive",
        "Get active profile name",
        [&self](const RpcRequest& rq) -> RpcResult {
            (void)rq;
            const std::string name = self.activeProfileName();
            return ok_("profile.getActive", json{{"name", name}});
        });

    // profile.setActive -> params: { "name": "<profile>" }
    reg.add(
        "profile.setActive",
        "Set active profile",
        [&self](const RpcRequest& rq) -> RpcResult {
            LOG_TRACE("rpc profile.setActive params=%s", rq.params.dump().c_str());
            json p = params_to_json(rq.params);
            if (!p.contains("name") || !p["name"].is_string()) {
                return err_("profile.setActive", -32602, "missing 'name'");
            }
            const std::string name = to_profile_name(p["name"].get<std::string>());
            self.setActiveProfileName(name); // void in Daemon
            return ok_("profile.setActive", json{{"name", name}});
        });

    // profile.load -> params: { "name": "<profile>" }
    reg.add(
        "profile.load",
        "Load a profile by name",
        [&self](const RpcRequest& rq) -> RpcResult {
            LOG_TRACE("rpc profile.load params=%s", rq.params.dump().c_str());
            json p = params_to_json(rq.params);
            if (!p.contains("name") || !p["name"].is_string()) {
                return err_("profile.load", -32602, "missing 'name'");
            }
            const std::string name = to_profile_name(p["name"].get<std::string>());

            const std::string path = self.profilePathForName(name);
            Profile prof;
            try {
                prof = loadProfileFromFile(path);
            } catch (const std::exception& ex) {
                LOG_WARN("profile.load: %s", ex.what());
                return err_("profile.load", -32001, ex.what());
            }

            json d = prof;
            d["name"] = name;
            return ok_("profile.load", d);
        });

    // profile.save -> params: { "name": "<profile>", "profile": { ... } }
    reg.add(
        "profile.save",
        "Save a profile",
        [&self](const RpcRequest& rq) -> RpcResult {
            LOG_TRACE("rpc profile.save params=%s", rq.params.dump().c_str());
            json p = params_to_json(rq.params);
            if (!p.contains("name") || !p["name"].is_string()) {
                return err_("profile.save", -32602, "missing 'name'");
            }
            if (!p.contains("profile") || !p["profile"].is_object()) {
                return err_("profile.save", -32602, "missing 'profile' object");
            }
            const std::string name = to_profile_name(p["name"].get<std::string>());

            Profile prof;
            try {
                from_json(p["profile"], prof);
            } catch (...) {
                return err_("profile.save", -32602, "invalid profile payload");
            }

            const std::string path = self.profilePathForName(name);
            try {
                saveProfileToFile(prof, path);
            } catch (const std::exception& ex) {
                LOG_WARN("profile.save: %s", ex.what());
                return err_("profile.save", -32003, ex.what());
            }
            return ok_("profile.save", json{{"name", name}});
        });

    // profile.delete -> params: { "name": "<profile>" }
    reg.add(
        "profile.delete",
        "Delete a profile",
        [&self](const RpcRequest& rq) -> RpcResult {
            LOG_TRACE("rpc profile.delete params=%s", rq.params.dump().c_str());
            json p = params_to_json(rq.params);
            if (!p.contains("name") || !p["name"].is_string()) {
                return err_("profile.delete", -32602, "missing 'name'");
            }
            const std::string name = to_profile_name(p["name"].get<std::string>());

            const std::filesystem::path path = self.profilePathForName(name);
            std::error_code ec;
            if (!std::filesystem::remove(path, ec) || ec) {
                std::string msg = "delete failed";
                if (ec) msg += (": " + ec.message());
                LOG_WARN("profile.delete: %s", msg.c_str());
                return err_("profile.delete", -32004, msg);
            }
            return ok_("profile.delete", json{{"name", name}});
        });

    // profile.rename -> params: { "from": "<old>", "to": "<new>" }
    reg.add(
        "profile.rename",
        "Rename a profile",
        [&self](const RpcRequest& rq) -> RpcResult {
            LOG_TRACE("rpc profile.rename params=%s", rq.params.dump().c_str());
            json p = params_to_json(rq.params);
            if (!p.contains("from") || !p["from"].is_string() ||
                !p.contains("to")   || !p["to"].is_string()) {
                return err_("profile.rename", -32602, "missing 'from' or 'to'");
            }
            const std::string from = to_profile_name(p["from"].get<std::string>());
            const std::string to   = to_profile_name(p["to"].get<std::string>());

            const std::filesystem::path oldp = self.profilePathForName(from);
            const std::filesystem::path newp = self.profilePathForName(to);

            std::error_code ec;
            std::filesystem::rename(oldp, newp, ec);
            if (ec) {
                std::string msg = "rename failed: " + ec.message();
                LOG_WARN("profile.rename: %s", msg.c_str());
                return err_("profile.rename", -32005, msg);
            }
            return ok_("profile.rename", json{{"from", from}, {"to", to}});
        });

    // profile.list -> returns { "profiles": [ {file, name}... ], "active": "<name>" }
    reg.add(
        "profile.list",
        "List profiles",
        [&self](const RpcRequest& rq) -> RpcResult {
            LOG_TRACE("rpc profile.list params=%s", rq.params.dump().c_str());
            (void)rq;

            json arr = json::array();
            try {
                const std::string dirStr = self.profilesDirPath();
                const std::filesystem::path dir = dirStr;
                if (!dir.empty() && std::filesystem::exists(dir)) {
                    for (const auto& entry : std::filesystem::directory_iterator(dir)) {
                        if (!entry.is_regular_file()) continue;
                        const auto& p = entry.path();
                        if (p.extension() == ".json") {
                            arr.push_back(json{
                                {"file", p.filename().string()},
                                {"name", p.stem().string()}
                            });
                        }
                    }
                }
            } catch (...) {
                LOG_WARN("profile.list encountered an exception while scanning directory");
            }
            return ok_("profile.list", json{{"profiles", arr}, {"active", self.activeProfileName()}});
        });
}

} // namespace lfc
