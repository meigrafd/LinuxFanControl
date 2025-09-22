/*
 * Linux Fan Control — RPC: profile.*
 * (c) 2025 LinuxFanControl contributors
 */

#include <nlohmann/json.hpp>
#include <string>
#include <filesystem>
#include <system_error>

#include "include/Daemon.hpp"
#include "include/CommandRegistry.hpp"   // ok_ / err_
#include "include/Profile.hpp"
#include "include/Log.hpp"

namespace lfc {

using nlohmann::json;
namespace fs = std::filesystem;

void BindRpcProfile(Daemon& self, CommandRegistry& reg) {
    // profile.getActive -> { name }
    reg.add(
        "profile.getActive",
        "Get active profile name",
        [&self](const RpcRequest& rq) -> RpcResult {
            LOG_TRACE("rpc profile.getActive");
            return ok_(rq, "profile.getActive", json{{"name", self.activeProfileName()}});
        }
    );

    // profile.setActive { name } -> { name }
    reg.add(
        "profile.setActive",
        "Set active profile name (does not auto-apply)",
        [&self](const RpcRequest& rq) -> RpcResult {
            LOG_TRACE("rpc profile.setActive params=%s", rq.params.dump().c_str());
            const json p = paramsToJson(rq);
            if (!p.contains("name") || !p.at("name").is_string()) {
                return err_(rq, "profile.setActive", -32602, "missing 'name'");
            }
            const std::string name = p.at("name").get<std::string>();
            self.setActiveProfileName(name);
            return ok_(rq, "profile.setActive", json{{"name", name}});
        }
    );

    // profile.load { name } -> profile object (+ name)
    reg.add(
        "profile.load",
        "Load a profile by name",
        [&self](const RpcRequest& rq) -> RpcResult {
            LOG_TRACE("rpc profile.load params=%s", rq.params.dump().c_str());
            const json p = paramsToJson(rq);
            if (!p.contains("name") || !p.at("name").is_string()) {
                return err_(rq, "profile.load", -32602, "missing 'name'");
            }
            const std::string name = p.at("name").get<std::string>();
            const std::string path = self.profilePathForName(name);
            try {
                Profile prof = loadProfileFromFile(path);
                json j = prof; // to_json(Profile)
                j["name"] = name;
                return ok_(rq, "profile.load", j);
            } catch (const std::exception& ex) {
                LOG_WARN("profile.load: %s", ex.what());
                return err_(rq, "profile.load", -32004, ex.what());
            }
        }
    );

    // profile.save { name, profile:{...} } -> { name, saved:true }
    reg.add(
        "profile.save",
        "Save a profile",
        [&self](const RpcRequest& rq) -> RpcResult {
            LOG_TRACE("rpc profile.save params=%s", rq.params.dump().c_str());
            const json p = paramsToJson(rq);
            if (!p.contains("name") || !p.at("name").is_string()) {
                return err_(rq, "profile.save", -32602, "missing 'name'");
            }
            if (!p.contains("profile") || !p.at("profile").is_object()) {
                return err_(rq, "profile.save", -32602, "missing 'profile' object");
            }
            const std::string name = p.at("name").get<std::string>();
            const std::string path = self.profilePathForName(name);
            try {
                Profile prof = p.at("profile").get<Profile>();
                saveProfileToFile(prof, path);
                return ok_(rq, "profile.save", json{{"name", name}, {"saved", true}});
            } catch (const std::exception& ex) {
                LOG_WARN("profile.save: %s", ex.what());
                return err_(rq, "profile.save", -32002, ex.what());
            }
        }
    );

    // profile.delete { name } -> { name }
    reg.add(
        "profile.delete",
        "Delete a profile file",
        [&self](const RpcRequest& rq) -> RpcResult {
            LOG_TRACE("rpc profile.delete params=%s", rq.params.dump().c_str());
            const json p = paramsToJson(rq);
            if (!p.contains("name") || !p.at("name").is_string()) {
                return err_(rq, "profile.delete", -32602, "missing 'name'");
            }
            const std::string name = p.at("name").get<std::string>();
            const std::string path = self.profilePathForName(name);

            std::error_code ec;
            if (!fs::exists(path, ec) || ec) {
                return err_(rq, "profile.delete", -32004, "profile not found");
            }
            if (!fs::remove(path, ec) || ec) {
                return err_(rq, "profile.delete", -32004,
                            ec ? ec.message() : std::string("delete failed"));
            }
            return ok_(rq, "profile.delete", json{{"name", name}});
        }
    );

    // profile.rename { from, to } -> { from, to }
    reg.add(
        "profile.rename",
        "Rename a profile file",
        [&self](const RpcRequest& rq) -> RpcResult {
            LOG_TRACE("rpc profile.rename params=%s", rq.params.dump().c_str());
            const json p = paramsToJson(rq);
            if (!p.contains("from") || !p.at("from").is_string() ||
                !p.contains("to")   || !p.at("to").is_string()) {
                return err_(rq, "profile.rename", -32602, "missing 'from'/'to'");
            }

            const std::string from = p.at("from").get<std::string>();
            const std::string to   = p.at("to").get<std::string>();
            const std::string src  = self.profilePathForName(from);
            const std::string dst  = self.profilePathForName(to);

            std::error_code ec;
            if (!fs::exists(src, ec) || ec) {
                return err_(rq, "profile.rename", -32004, "source profile not found");
            }
            // Try rename; fallback to copy+remove for cross-device moves
            fs::rename(src, dst, ec);
            if (ec) {
                ec.clear();
                fs::copy_file(src, dst, fs::copy_options::overwrite_existing, ec);
                if (ec) return err_(rq, "profile.rename", -32004, ec.message());
                ec.clear();
                fs::remove(src, ec);
                if (ec) return err_(rq, "profile.rename", -32004, ec.message());
            }

            if (self.activeProfileName() == from) {
                self.setActiveProfileName(to);
            }
            return ok_(rq, "profile.rename", json{{"from", from}, {"to", to}});
        }
    );

    // profile.list -> { profiles:[{file,name}], active:"..." }
    reg.add(
        "profile.list",
        "List available profiles (+active name)",
        [&self](const RpcRequest& rq) -> RpcResult {
            (void)rq;
            LOG_TRACE("rpc profile.list");

            json arr = json::array();
            const std::string dir = self.profilesPath();
            try {
                if (!dir.empty() && fs::exists(dir)) {
                    for (const auto& e : fs::directory_iterator(dir)) {
                        if (!e.is_regular_file()) continue;
                        const auto p = e.path();
                        if (p.extension() == ".json") {
                            arr.push_back(json{
                                {"file", p.filename().string()},
                                {"name", p.stem().string()}
                            });
                        }
                    }
                }
            } catch (const std::exception& ex) {
                LOG_WARN("profile.list: %s", ex.what());
            }

            return ok_(rq, "profile.list", json{
                {"profiles", arr},
                {"active", self.activeProfileName()}
            });
        }
    );
}

} // namespace lfc
