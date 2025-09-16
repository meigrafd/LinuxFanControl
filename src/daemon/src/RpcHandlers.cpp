/*
 * Linux Fan Control — RPC handlers (implementation)
 * - Registers all JSON-RPC methods for the daemon
 * (c) 2025 LinuxFanControl contributors
 */

#include "RpcHandlers.hpp"

#include "Daemon.hpp"
#include "Config.hpp"
#include "include/CommandRegistry.h"
#include "FanControlImport.hpp"
#include "UpdateChecker.hpp"
#include "Version.hpp"
#include "Engine.hpp"

#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <cctype>

using nlohmann::json;

namespace lfc {

// ---- helpers ----

static inline std::string jstr(const std::string& s) {
    std::string out; out.reserve(s.size() + 2);
    out.push_back('"');
    for (char c : s) {
        if (c == '\\' || c == '"') { out.push_back('\\'); out.push_back(c); }
        else if (c == '\n') out += "\\n";
        else out.push_back(c);
    }
    out.push_back('"');
    return out;
}

static inline RpcResult ok(const char* method, const std::string& dataJson) {
    std::ostringstream ss;
    ss << "{\"method\":" << jstr(method) << ",\"success\":true";
    if (!dataJson.empty()) ss << ",\"data\":" << dataJson;
    ss << "}";
    return {true, ss.str()};
}

static inline RpcResult err(const char* method, int code, const std::string& msg) {
    std::ostringstream ss;
    ss << "{\"method\":" << jstr(method) << ",\"success\":false,"
       << "\"error\":{\"code\":" << code << ",\"message\":" << jstr(msg) << "}}";
    return {false, ss.str()};
}

static int clampi(int v, int lo, int hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}
static double clampd(double v, double lo, double hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

// ---- profile name/file helpers ----

static std::string to_profile_name(std::string s) {
    if (s.size() >= 5) {
        std::string tail = s.substr(s.size() - 5);
        for (char& c : tail) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (tail == ".json") s.erase(s.size() - 5);
    }
    return s;
}

static std::filesystem::path file_for_profile(const std::string& dir, const std::string& name) {
    return std::filesystem::path(dir) / (name + std::string(".json"));
}

// ---- command bindings ----

void BindDaemonRpcCommands(Daemon& self, CommandRegistry& reg) {
    reg.registerMethod("ping", "Health check", [&](const RpcRequest&) -> RpcResult {
        return ok("ping", "{}");
    });

    reg.registerMethod("version", "RPC and daemon version", [&](const RpcRequest&) -> RpcResult {
        json j;
        j["daemon"] = "lfcd";
        j["version"] = LFCD_VERSION;
        j["rpc"] = 1;
        return ok("version", j.dump());
    });

    reg.registerMethod("rpc.commands", "List available RPC methods", [&](const RpcRequest&) -> RpcResult {
        auto list = self.listRpcCommands();
        json arr = json::array();
        for (const auto& c : list) {
            arr.push_back({{"name", c.name}, {"help", c.help}});
        }
        return ok("rpc.commands", arr.dump());
    });

    reg.registerMethod("config.load", "Load daemon config from disk", [&](const RpcRequest&) -> RpcResult {
        std::string e;
        DaemonConfig tmp = loadDaemonConfig(self.configPath(), &e);
        if (!e.empty()) return err("config.load", -32002, "config load failed");

        json out;
        out["log"]["file"]           = tmp.logfile;
        out["log"]["maxBytes"]       = 5ull * 1024ull * 1024ull;
        out["log"]["rotateCount"]    = 3;
        out["log"]["debug"]          = tmp.debug;

        out["rpc"]["host"]           = tmp.host;
        out["rpc"]["port"]           = tmp.port;

        out["shm"]["path"]           = tmp.shmPath;

        out["profiles"]["dir"]       = tmp.profilesDir;
        out["profiles"]["active"]    = tmp.profileName;
        out["profiles"]["backups"]   = true;

        out["pidFile"]               = tmp.pidfile;

        out["engine"]["deltaC"]      = self.engineDeltaC();
        out["engine"]["forceTickMs"] = self.engineForceTickMs();
        out["engine"]["tickMs"]      = self.engineTickMs();

        return ok("config.load", out.dump());
    });

    reg.registerMethod("config.save", "Save daemon config; params:{config:object}", [&](const RpcRequest& rq) -> RpcResult {
        if (rq.params.empty()) return err("config.save", -32602, "bad params");
        json p = json::parse(rq.params, nullptr, false);
        if (p.is_discarded() || !p.contains("config") || !p["config"].is_object())
            return err("config.save", -32602, "missing config");

        DaemonConfig nc = self.cfg();
        auto& jc = p["config"];

        if (jc.contains("log")) {
            if (jc["log"].contains("file") && jc["log"]["file"].is_string()) nc.logfile = jc["log"]["file"].get<std::string>();
            if (jc["log"].contains("debug") && jc["log"]["debug"].is_boolean()) nc.debug = jc["log"]["debug"].get<bool>();
        }
        if (jc.contains("rpc")) {
            if (jc["rpc"].contains("host") && jc["rpc"]["host"].is_string()) nc.host = jc["rpc"]["host"].get<std::string>();
            if (jc["rpc"].contains("port") && jc["rpc"]["port"].is_number_integer()) nc.port = jc["rpc"]["port"].get<int>();
        }
        if (jc.contains("shm")) {
            if (jc["shm"].contains("path") && jc["shm"]["path"].is_string()) nc.shmPath = jc["shm"]["path"].get<std::string>();
        }
        if (jc.contains("profiles")) {
            if (jc["profiles"].contains("dir") && jc["profiles"]["dir"].is_string()) {
                nc.profilesDir = jc["profiles"]["dir"].get<std::string>();
            }
            if (jc["profiles"].contains("active") && jc["profiles"]["active"].is_string()) {
                nc.profileName = to_profile_name(jc["profiles"]["active"].get<std::string>());
            }
        }
        if (jc.contains("pidFile") && jc["pidFile"].is_string()) {
            nc.pidfile = jc["pidFile"].get<std::string>();
        }
        if (jc.contains("engine") && jc["engine"].is_object()) {
            if (jc["engine"].contains("deltaC") && jc["engine"]["deltaC"].is_number()) {
                double v = clampd(jc["engine"]["deltaC"].get<double>(), 0.0, 10.0);
                nc.deltaC = v;
                self.setEngineDeltaC(v);
            }
            if (jc["engine"].contains("forceTickMs") && jc["engine"]["forceTickMs"].is_number_integer()) {
                int v = clampi(jc["engine"]["forceTickMs"].get<int>(), 100, 10000);
                nc.forceTickMs = v;
                self.setEngineForceTickMs(v);
            }
            if (jc["engine"].contains("tickMs") && jc["engine"]["tickMs"].is_number_integer()) {
                int v = clampi(jc["engine"]["tickMs"].get<int>(), 5, 1000);
                nc.tickMs = v;
                self.setEngineTickMs(v);
            }
        }

        std::string e;
        if (!saveDaemonConfig(nc, self.configPath(), &e)) return err("config.save", -32003, e);
        self.setCfg(nc);
        return ok("config.save", "{}");
    });

    reg.registerMethod("config.set", "Set single key; params:{key,value}", [&](const RpcRequest& rq) -> RpcResult {
        if (rq.params.empty()) return err("config.set", -32602, "bad params");
        json p = json::parse(rq.params, nullptr, false);
        if (p.is_discarded() || !p.contains("key")) return err("config.set", -32602, "missing key");

        std::string key = p["key"].get<std::string>();
        json val = p.contains("value") ? p["value"] : json();

        DaemonConfig nc = self.cfg();

        if (key == "log.debug") {
            if (val.is_boolean()) nc.debug = val.get<bool>();
        } else if (key == "log.file") {
            if (val.is_string()) nc.logfile = val.get<std::string>();
        } else if (key == "rpc.host") {
            if (val.is_string()) nc.host = val.get<std::string>();
        } else if (key == "rpc.port") {
            if (val.is_number_integer()) nc.port = val.get<int>();
        } else if (key == "shm.path") {
            if (val.is_string()) nc.shmPath = val.get<std::string>();
        } else if (key == "profiles.dir") {
            if (val.is_string()) nc.profilesDir = val.get<std::string>();
        } else if (key == "profiles.active") {
            if (val.is_string()) nc.profileName = to_profile_name(val.get<std::string>());
        } else if (key == "pidFile") {
            if (val.is_string()) nc.pidfile = val.get<std::string>();
        } else if (key == "engine.deltaC") {
            double v = self.engineDeltaC();
            if (val.is_number()) v = val.get<double>();
            else if (val.is_string()) { try { v = std::stod(val.get<std::string>()); } catch (...) {} }
            v = clampd(v, 0.0, 10.0);
            nc.deltaC = v;
            self.setEngineDeltaC(v);
        } else if (key == "engine.forceTickMs") {
            int v = self.engineForceTickMs();
            if (val.is_number_integer()) v = val.get<int>();
            else if (val.is_string()) { try { v = std::stoi(val.get<std::string>()); } catch (...) {} }
            v = clampi(v, 100, 10000);
            nc.forceTickMs = v;
            self.setEngineForceTickMs(v);
        } else if (key == "engine.tickMs") {
            int v = self.engineTickMs();
            if (val.is_number_integer()) v = val.get<int>();
            else if (val.is_string()) { try { v = std::stoi(val.get<std::string>()); } catch (...) {} }
            v = clampi(v, 5, 1000);
            nc.tickMs = v;
            self.setEngineTickMs(v);
        } else {
            return err("config.set", -32602, "unknown key");
        }

        std::string e;
        if (!saveDaemonConfig(nc, self.configPath(), &e)) return err("config.set", -32003, e);
        self.setCfg(nc);
        return ok("config.set", "{}");
    });

    // ---- profiles ----

    reg.registerMethod("profile.import", "Import FanControl.Releases; params:{path}", [&](const RpcRequest& rq) -> RpcResult {
        if (rq.params.empty()) return err("profile.import", -32602, "bad params");
        json p = json::parse(rq.params, nullptr, false);
        if (p.is_discarded() || !p.contains("path")) return err("profile.import", -32602, "missing path");

        std::string path = p["path"].get<std::string>();
        std::string e;
        Profile prof;
        if (!FanControlImport::LoadAndMap(path, self.hwmon(), prof, e)) return err("profile.import", -32010, e);

        self.engine().applyProfile(prof);

        DaemonConfig nc = self.cfg();
        nc.profileName = prof.name.empty() ? std::string("Imported") : prof.name;
        std::string serr;
        (void)saveDaemonConfig(nc, self.configPath(), &serr);
        self.setCfg(nc);

        return ok("profile.import", "{}");
    });

    reg.registerMethod("profile.importAs",
        "Import FanControl.Releases and save; params:{path,name}",
        [&](const RpcRequest& rq) -> RpcResult {
            if (rq.params.empty()) return err("profile.importAs", -32602, "bad params");
            json p = json::parse(rq.params, nullptr, false);
            if (p.is_discarded() || !p.contains("path") || !p.contains("name"))
                return err("profile.importAs", -32602, "missing path or name");

            const std::string srcPath = p["path"].get<std::string>();
            std::string nameOnly = to_profile_name(p["name"].get<std::string>());
            if (nameOnly.empty()) return err("profile.importAs", -32602, "empty name");

            Profile prof;
            std::string ierr;
            if (!FanControlImport::LoadAndMap(srcPath, self.hwmon(), prof, ierr))
                return err("profile.importAs", -32010, ierr);

            prof.name = nameOnly;

            const std::string dir = self.cfg().profilesDir;
            if (dir.empty()) return err("profile.importAs", -32011, "profilesDir not configured");

            std::error_code ec;
            std::filesystem::create_directories(dir, ec);

            auto dst = file_for_profile(dir, nameOnly);
            std::string serr;
            if (!prof.saveToFile(dst.string(), &serr))
                return err("profile.importAs", -32012, serr);

            if (!self.applyProfile(prof))
                return err("profile.importAs", -32013, "apply failed");

            DaemonConfig nc = self.cfg();
            nc.profileName = nameOnly;
            (void)saveDaemonConfig(nc, self.configPath(), &serr);
            self.setCfg(nc);

            json data;
            data["savedPath"] = dst.string();
            data["name"] = nameOnly;
            data["applied"] = true;
            return ok("profile.importAs", data.dump());
        });

    reg.registerMethod("profile.load", "Load/apply profile; params:{name}", [&](const RpcRequest& rq) -> RpcResult {
        if (rq.params.empty()) return err("profile.load", -32602, "bad params");
        json p = json::parse(rq.params, nullptr, false);
        if (p.is_discarded() || !p.contains("name")) return err("profile.load", -32602, "missing name");

        std::string nameOnly = to_profile_name(p["name"].get<std::string>());
        auto full = file_for_profile(self.cfg().profilesDir, nameOnly);
        if (!std::filesystem::exists(full)) return err("profile.load", -32004, "profile not found");
        if (!self.applyProfileFile(full.string())) return err("profile.load", -32005, "invalid profile");

        DaemonConfig nc = self.cfg();
        nc.profileName = nameOnly;
        std::string serr;
        (void)saveDaemonConfig(nc, self.configPath(), &serr);
        self.setCfg(nc);

        return ok("profile.load", "{}");
    });

    reg.registerMethod("profile.set", "Write profile file; params:{name,profile}", [&](const RpcRequest& rq) -> RpcResult {
        if (rq.params.empty()) return err("profile.set", -32602, "bad params");
        json p = json::parse(rq.params, nullptr, false);
        if (p.is_discarded() || !p.contains("name") || !p.contains("profile")) return err("profile.set", -32602, "missing name/profile");

        std::string nameOnly = to_profile_name(p["name"].get<std::string>());
        json prof = p["profile"];

        auto path = file_for_profile(self.cfg().profilesDir, nameOnly);
        std::error_code ec;
        std::filesystem::create_directories(path.parent_path(), ec);
        if (ec) return err("profile.set", -32006, ec.message());

        std::ofstream f(path);
        if (!f) return err("profile.set", -32006, "open failed");
        f << prof.dump(2) << "\n";
        f.close();
        return ok("profile.set", "{}");
    });

    reg.registerMethod("profile.delete", "Delete profile; params:{name}", [&](const RpcRequest& rq) -> RpcResult {
        if (rq.params.empty()) return err("profile.delete", -32602, "bad params");
        json p = json::parse(rq.params, nullptr, false);
        if (p.is_discarded() || !p.contains("name")) return err("profile.delete", -32602, "missing name");

        std::string nameOnly = to_profile_name(p["name"].get<std::string>());
        auto path = file_for_profile(self.cfg().profilesDir, nameOnly);
        std::error_code ec;
        bool okDel = std::filesystem::remove(path, ec);
        if (!okDel && ec) return err("profile.delete", -32007, ec.message());
        return ok("profile.delete", "{}");
    });

    // ---- lists / info ----

    reg.registerMethod("list.profiles", "List profile names", [&](const RpcRequest&) -> RpcResult {
        json arr = json::array();
        const std::string dir = self.cfg().profilesDir;
        if (!dir.empty() && std::filesystem::exists(dir)) {
            for (auto& e : std::filesystem::directory_iterator(dir)) {
                if (!e.is_regular_file()) continue;
                auto p = e.path().filename().string();
                if (p.size() >= 5 && p.substr(p.size() - 5) == ".json") p.erase(p.size() - 5);
                arr.push_back(p);
            }
        }
        return ok("list.profiles", arr.dump());
    });

    reg.registerMethod("telemetry.json", "Return SHM JSON", [&](const RpcRequest&) -> RpcResult {
        std::string s;
        if (!self.telemetryGet(s)) s = "{}";
        return ok("telemetry.json", s);
    });

    // ---- daemon / update ----

    reg.registerMethod("daemon.update", "Check/download latest release", [&](const RpcRequest& rq) -> RpcResult {
        std::string repoOwner = "meigrafd";
        std::string repo = "LinuxFanControl";

        if (!rq.params.empty()) {
            json p = json::parse(rq.params, nullptr, false);
            if (!p.is_discarded() && p.contains("repo") && p["repo"].is_string()) {
                auto s = p["repo"].get<std::string>();
                auto pos = s.find('/');
                if (pos != std::string::npos) {
                    repoOwner = s.substr(0, pos);
                    repo = s.substr(pos + 1);
                }
            }
        }

        ReleaseInfo rel;
        std::string e;
        if (!UpdateChecker::fetchLatest(repoOwner, repo, rel, e)) return err("daemon.update", -32020, e);
        int cmp = UpdateChecker::compareVersions(LFCD_VERSION, rel.tag);

        json out;
        out["current"] = LFCD_VERSION;
        out["latest"] = rel.tag;
        out["name"] = rel.name;
        out["html"] = rel.htmlUrl;
        out["updateAvailable"] = (cmp < 0);
        out["assets"] = json::array();
        for (auto& a : rel.assets) {
            out["assets"].push_back({{"name", a.name}, {"url", a.url}, {"type", a.type}, {"size", a.size}});
        }

        bool dl = false;
        std::string target;
        if (!rq.params.empty()) {
            json p = json::parse(rq.params, nullptr, false);
            if (!p.is_discarded()) {
                dl = p.value("download", false);
                if (p.contains("target") && p["target"].is_string()) target = p["target"].get<std::string>();
            }
        }
        if (dl) {
            if (target.empty()) return err("daemon.update", -32602, "missing target");
            std::string url;
            for (const auto& a : rel.assets) {
                std::string n = a.name;
                for (auto& c : n) c = static_cast<char>(tolower(c));
                if (n.find("linux") != std::string::npos &&
                    (n.find("x86_64") != std::string::npos || n.find("amd64") != std::string::npos)) {
                    url = a.url;
                    break;
                }
            }
            if (url.empty() && !rel.assets.empty()) url = rel.assets[0].url;
            if (url.empty()) return err("daemon.update", -32021, "no assets");
            if (!UpdateChecker::downloadToFile(url, target, e)) return err("daemon.update", -32022, e);
            out["downloaded"] = true;
            out["target"] = target;
        }

        return ok("daemon.update", out.dump());
    });

    reg.registerMethod("daemon.restart", "Request restart", [&](const RpcRequest&) -> RpcResult {
        return ok("daemon.restart", "{}");
    });

    reg.registerMethod("daemon.shutdown", "Shutdown daemon", [&](const RpcRequest&) -> RpcResult {
        self.requestStop();
        return ok("daemon.shutdown", "{}");
    });
}

} // namespace lfc
