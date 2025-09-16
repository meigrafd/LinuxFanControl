/*
 * Linux Fan Control — Daemon (implementation)
 * - JSON-RPC (TCP) server lifecycle
 * - SHM telemetry + engine bootstrap
 * - PID file handling (/run primary, /tmp fallback persisted)
 * - Config/profile bootstrap and import mapping
 * - Non-blocking detection worker
 * (c) 2025 LinuxFanControl contributors
 */
#include "Daemon.hpp"
#include "RpcTcpServer.hpp"
#include "Hwmon.hpp"
#include "FanControlImport.hpp"
#include "Config.hpp"
#include "include/CommandRegistry.h"
#include "Log.hpp"
#include "Version.hpp"
#include "UpdateChecker.hpp"

#include <nlohmann/json.hpp>

#include <fstream>
#include <sstream>
#include <thread>
#include <filesystem>
#include <chrono>
#include <cstring>
#include <unistd.h>
#include <sys/stat.h>

namespace lfc {

using nlohmann::json;

Daemon::Daemon() = default;
Daemon::~Daemon() { shutdown(); }

static bool ensureDir(const std::string& path) {
    std::error_code ec;
    auto dir = std::filesystem::path(path).parent_path();
    if (dir.empty()) return true;
    std::filesystem::create_directories(dir, ec);
    return !ec;
}

bool Daemon::writePidFile(const std::string& path) {
    if (!ensureDir(path)) return false;
    std::ofstream f(path, std::ios::trunc);
    if (!f) return false;
    f << getpid() << "\n";
    pidFile_ = path;
    return true;
}

void Daemon::removePidFile() {
    if (pidFile_.empty()) return;
    std::error_code ec;
    std::filesystem::remove(pidFile_, ec);
    pidFile_.clear();
}

bool Daemon::applyProfileIfValid(const std::string& profilePath) {
    std::string err;
    Profile p;
    if (!ProfileIO::Load(profilePath, p, err)) {
        LFC_LOGW("profile load failed: %s", err.c_str());
        return false;
    }
    if (!ProfileIO::Validate(p, hwmon_, err)) {
        LFC_LOGW("profile invalid: %s", err.c_str());
        return false;
    }
    engine_.applyProfile(p);
    LFC_LOGI("profile applied: %s", profilePath.c_str());
    return true;
}

bool Daemon::init(DaemonConfig& cfg, bool debugCli, const std::string& cfgPath) {
    configPath_ = cfgPath;
    cfg_ = cfg;

    Logger::instance().configure(cfg.log.file, cfg.log.maxBytes, cfg.log.rotateCount, cfg.log.debug || debugCli);
    if (cfg.log.debug || debugCli) Logger::instance().setLevel(LogLevel::Debug);

    std::string primary = cfg.pidFile.empty() ? std::string("/run/lfcd.pid") : cfg.pidFile;
    if (!writePidFile(primary)) {
        std::string fallback{"/tmp/lfcd.pid"};
        if (writePidFile(fallback)) {
            cfg.pidFile = fallback;
            cfg_.pidFile = fallback;
            std::string err;
            (void)Config::Save(cfgPath, cfg_, err);
            LFC_LOGI("pidfile fallback to %s (persisted)", fallback.c_str());
        } else {
            LFC_LOGW("pidfile create failed (primary: %s, fallback: %s)", primary.c_str(), fallback.c_str());
        }
    }

    hwmon_ = Hwmon::scan();
    engine_.setSnapshot(hwmon_);
    engine_.initShm(cfg.shm.path);

    std::filesystem::path profPath = std::filesystem::path(cfg.profiles.dir) / cfg.profiles.active;
    if (std::filesystem::exists(profPath)) {
        applyProfileIfValid(profPath.string());
    } else {
        LFC_LOGI("no profile present at %s — awaiting detection/import via RPC", profPath.string().c_str());
        engine_.enableControl(false);
    }

    reg_ = std::make_unique<CommandRegistry>();
    bindCommands(*reg_);

    rpc_ = std::make_unique<RpcTcpServer>(*this, cfg.rpc.host, static_cast<std::uint16_t>(cfg.rpc.port), (cfg.log.debug || debugCli));
    if (!rpc_->start(reg_.get())) {
        LFC_LOGE("rpc start failed");
        return false;
    }

    running_ = true;
    return true;
}

void Daemon::shutdown() {
    if (!running_) return;
    running_ = false;
    if (rpc_) { rpc_->stop(); rpc_.reset(); }
    {
        std::lock_guard<std::mutex> lk(detMu_);
        if (detection_) detection_->abort();
    }
    engine_.stop();
    removePidFile();
}

void Daemon::runLoop() {
    engine_.start();
    while (running_) pumpOnce();
}

void Daemon::pumpOnce(int /*timeoutMs*/) {
    engine_.tick();
    std::lock_guard<std::mutex> lk(detMu_);
    if (detection_) detection_->poll();
}

static std::string jsonStr(const std::string& s) {
    std::string out; out.reserve(s.size() + 2); out.push_back('"');
    for (char c : s) { if (c == '"' || c == '\\') out.push_back('\\'); out.push_back(c); }
    out.push_back('"'); return out;
}

void Daemon::bindCommands(CommandRegistry& reg) {
    // Helpers
    auto ok = [&](const char* method, const std::string& extra) -> RpcResult {
        std::ostringstream ss;
        ss << "{\"method\":" << jsonStr(method) << ",\"ok\":true";
        if (!extra.empty()) ss << "," << extra;
        ss << "}";
        return {true, ss.str()};
    };
    auto err = [&](const char* method, int code, const std::string& msg) -> RpcResult {
        std::ostringstream ss;
        ss << "{\"method\":" << jsonStr(method) << ",\"ok\":false,\"code\":" << code << ",\"message\":" << jsonStr(msg) << "}";
        return {false, ss.str()};
    };

    // ping
    reg.registerMethod("ping", "Health check", [&](const RpcRequest&) -> RpcResult {
        return ok("ping", "\"pong\":true");
    });

    // version
    reg.registerMethod("version", "RPC and daemon version", [&](const RpcRequest&) -> RpcResult {
        std::ostringstream x;
        x << "\"daemon\":\"lfcd\",\"version\":" << jsonStr(LFC_VERSION) << ",\"rpc\":1";
        return ok("version", x.str());
    });

    // rpc.commands
    reg.registerMethod("rpc.commands", "List available RPC methods", [&](const RpcRequest&) -> RpcResult {
        auto list = this->listRpcCommands();
        std::ostringstream body;
        body << "\"methods\":[";
        for (size_t i = 0; i < list.size(); ++i) {
            if (i) body << ",";
            body << "{\"name\":" << jsonStr(list[i].name)
                 << ",\"help\":" << jsonStr(list[i].help) << "}";
        }
        body << "]";
        return ok("rpc.commands", body.str());
    });

    // config.load
    reg.registerMethod("config.load", "Load daemon config from disk", [&](const RpcRequest&) -> RpcResult {
        DaemonConfig tmp;
        std::string e;
        if (!Config::Load(this->configPath_, tmp, e)) return err("config.load", -32002, "config load failed");
        json j; std::string serr;
        Config::Save("/dev/null", tmp, serr); // build structure below manually to avoid /dev/null write dependency
        json out;
        out["log"]["file"] = tmp.log.file;
        out["log"]["maxBytes"] = static_cast<std::uint64_t>(tmp.log.maxBytes);
        out["log"]["rotateCount"] = tmp.log.rotateCount;
        out["log"]["debug"] = tmp.log.debug;
        out["rpc"]["host"] = tmp.rpc.host;
        out["rpc"]["port"] = tmp.rpc.port;
        out["shm"]["path"] = tmp.shm.path;
        out["profiles"]["dir"] = tmp.profiles.dir;
        out["profiles"]["active"] = tmp.profiles.active;
        out["profiles"]["backups"] = tmp.profiles.backups;
        out["pidFile"] = tmp.pidFile;
        std::ostringstream body;
        body << "\"config\":" << out.dump();
        return ok("config.load", body.str());
    });

    // config.save
    reg.registerMethod("config.save", "Save daemon config to disk; params:{config:object}", [&](const RpcRequest& req) -> RpcResult {
        json p;
        try { p = json::parse(req.paramsJson.empty() ? "{}" : req.paramsJson); }
        catch (...) { return err("config.save", -32602, "bad params"); }
        if (!p.contains("config") || !p["config"].is_object()) return err("config.save", -32602, "missing config");
        DaemonConfig nc = cfg_;
        auto& jc = p["config"];
        if (jc.contains("log")) {
            nc.log.file = jc["log"].value("file", nc.log.file);
            nc.log.maxBytes = static_cast<std::size_t>(jc["log"].value("maxBytes", static_cast<std::uint64_t>(nc.log.maxBytes)));
            nc.log.rotateCount = jc["log"].value("rotateCount", nc.log.rotateCount);
            nc.log.debug = jc["log"].value("debug", nc.log.debug);
        }
        if (jc.contains("rpc")) {
            nc.rpc.host = jc["rpc"].value("host", nc.rpc.host);
            nc.rpc.port = jc["rpc"].value("port", nc.rpc.port);
        }
        if (jc.contains("shm")) {
            nc.shm.path = jc["shm"].value("path", nc.shm.path);
        }
        if (jc.contains("profiles")) {
            nc.profiles.dir = jc["profiles"].value("dir", nc.profiles.dir);
            nc.profiles.active = jc["profiles"].value("active", nc.profiles.active);
            nc.profiles.backups = jc["profiles"].value("backups", nc.profiles.backups);
        }
        if (jc.contains("pidFile")) nc.pidFile = jc.value("pidFile", nc.pidFile);

        std::string e;
        if (!Config::Save(this->configPath_, nc, e)) return err("config.save", -32003, "config save failed");
        cfg_ = nc;
        return ok("config.save", "\"saved\":true");
    });

    // config.set (partial update)
    reg.registerMethod("config.set", "Set config keys; params:{key:..., value:...}", [&](const RpcRequest& req) -> RpcResult {
        json p;
        try { p = json::parse(req.paramsJson.empty() ? "{}" : req.paramsJson); }
        catch (...) { return err("config.set", -32602, "bad params"); }
        if (!p.contains("key")) return err("config.set", -32602, "missing key");
        std::string key = p["key"].get<std::string>();
        json val = p.contains("value") ? p["value"] : json();

        DaemonConfig nc = cfg_;
        if (key == "log.debug") nc.log.debug = val.is_boolean() ? val.get<bool>() : nc.log.debug;
        else if (key == "log.file") nc.log.file = val.is_string() ? val.get<std::string>() : nc.log.file;
        else if (key == "rpc.host") nc.rpc.host = val.is_string() ? val.get<std::string>() : nc.rpc.host;
        else if (key == "rpc.port") nc.rpc.port = val.is_number_integer() ? val.get<int>() : nc.rpc.port;
        else if (key == "shm.path") nc.shm.path = val.is_string() ? val.get<std::string>() : nc.shm.path;
        else if (key == "profiles.dir") nc.profiles.dir = val.is_string() ? val.get<std::string>() : nc.profiles.dir;
        else if (key == "profiles.active") nc.profiles.active = val.is_string() ? val.get<std::string>() : nc.profiles.active;
        else if (key == "pidFile") nc.pidFile = val.is_string() ? val.get<std::string>() : nc.pidFile;
        else return err("config.set", -32602, "unknown key");

        std::string e;
        if (!Config::Save(this->configPath_, nc, e)) return err("config.set", -32003, "config save failed");
        cfg_ = nc;
        std::ostringstream body;
        body << "\"key\":" << jsonStr(key) << ",\"set\":true";
        return ok("config.set", body.str());
    });

    // config.import (FanControl.Releases)
    reg.registerMethod("config.import", "Import FanControl.Releases config; params:{\"path\":\"...\"}", [&](const RpcRequest& req) -> RpcResult {
        json p;
        try { p = json::parse(req.paramsJson.empty() ? "{}" : req.paramsJson); }
        catch (...) { return err("config.import", -32602, "bad params"); }
        if (!p.contains("path")) return err("config.import", -32602, "missing path");
        std::string path = p["path"].get<std::string>();

        std::string e;
        Profile prof;
        if (!FanControlImport::LoadAndMap(path, hwmon_, prof, e)) return err("config.import", -32010, e);
        engine_.applyProfile(prof);
        return ok("config.import", "\"applied\":true");
    });

    // profile.load
    reg.registerMethod("profile.load", "Load and apply profile by name; params:{\"name\":\"...\"}", [&](const RpcRequest& req) -> RpcResult {
        json p;
        try { p = json::parse(req.paramsJson.empty() ? "{}" : req.paramsJson); }
        catch (...) { return err("profile.load", -32602, "bad params"); }
        if (!p.contains("name")) return err("profile.load", -32602, "missing name");
        std::string name = p["name"].get<std::string>();
        std::filesystem::path full = std::filesystem::path(cfg_.profiles.dir) / name;
        if (!std::filesystem::exists(full)) return err("profile.load", -32004, "profile not found");
        bool okv = applyProfileIfValid(full.string());
        if (!okv) return err("profile.load", -32005, "invalid profile");
        cfg_.profiles.active = name;
        std::string serr;
        (void)Config::Save(this->configPath_, cfg_, serr);
        return ok("profile.load", "\"active\":" + jsonStr(name));
    });

    // profile.set (write profile JSON as given)
    reg.registerMethod("profile.set", "Write profile JSON; params:{\"name\":\"...\",\"profile\":{...}}", [&](const RpcRequest& req) -> RpcResult {
        json p;
        try { p = json::parse(req.paramsJson.empty() ? "{}" : req.paramsJson); }
        catch (...) { return err("profile.set", -32602, "bad params"); }
        if (!p.contains("name") || !p.contains("profile")) return err("profile.set", -32602, "missing name/profile");
        std::string name = p["name"].get<std::string>();
        Profile prof;
        if (!ProfileIO::Load("/dev/stdin", prof, *(new std::string()))) {} // placeholder to satisfy ODR; not used
        // parse into Profile using existing loader: convert json->string and reuse ProfileIO::Load by temp file
        std::string tmp = p["profile"].dump();
        std::filesystem::path path = std::filesystem::path(cfg_.profiles.dir) / name;
        std::error_code ec;
        std::filesystem::create_directories(path.parent_path(), ec);
        std::ofstream f(path);
        if (!f) return err("profile.set", -32006, "open failed");
        f << tmp << "\n";
        f.close();
        return ok("profile.set", "\"saved\":" + jsonStr(path.string()));
    });

    // profile.delete
    reg.registerMethod("profile.delete", "Delete profile file; params:{\"name\":\"...\"}", [&](const RpcRequest& req) -> RpcResult {
        json p;
        try { p = json::parse(req.paramsJson.empty() ? "{}" : req.paramsJson); }
        catch (...) { return err("profile.delete", -32602, "bad params"); }
        if (!p.contains("name")) return err("profile.delete", -32602, "missing name");
        std::filesystem::path full = std::filesystem::path(cfg_.profiles.dir) / p["name"].get<std::string>();
        std::error_code ec;
        bool okrm = std::filesystem::remove(full, ec);
        if (!okrm || ec) return err("profile.delete", -32007, "delete failed");
        return ok("profile.delete", "\"deleted\":true");
    });

    // active.profile
    reg.registerMethod("active.profile", "Get active profile filename", [&](const RpcRequest&) -> RpcResult {
        return ok("active.profile", "\"active\":" + jsonStr(cfg_.profiles.active));
    });

    // list.profiles
    reg.registerMethod("list.profiles", "List profiles in profiles dir", [&](const RpcRequest&) -> RpcResult {
        json arr = json::array();
        std::error_code ec;
        for (auto& e : std::filesystem::directory_iterator(cfg_.profiles.dir, ec)) {
            if (!e.is_regular_file()) continue;
            if (e.path().extension() == ".json") arr.push_back(e.path().filename().string());
        }
        std::ostringstream body; body << "\"profiles\":" << arr.dump();
        return ok("list.profiles", body.str());
    });

    // detect.start
    reg.registerMethod("detect.start", "Start non-blocking detection", [&](const RpcRequest&) -> RpcResult {
        std::lock_guard<std::mutex> lk(this->detMu_);
        if (this->detection_ && this->detection_->running())
            return ok("detect.start", "\"started\":false,\"reason\":\"already_running\"");
        this->detection_ = std::make_unique<Detection>(this->hwmon_);
        this->detection_->start();
        return ok("detect.start", "\"started\":true");
    });

    // detect.status
    reg.registerMethod("detect.status", "Detection status/progress", [&](const RpcRequest&) -> RpcResult {
        std::lock_guard<std::mutex> lk(this->detMu_);
        if (!this->detection_) return ok("detect.status", "\"running\":false");
        auto st = this->detection_->status();
        std::ostringstream body;
        body << "\"running\":" << (st.running ? "true" : "false")
             << ",\"current_index\":" << st.currentIndex
             << ",\"total\":" << st.total
             << ",\"phase\":" << jsonStr(st.phase);
        return ok("detect.status", body.str());
    });

    // detect.abort
    reg.registerMethod("detect.abort", "Abort detection", [&](const RpcRequest&) -> RpcResult {
        std::lock_guard<std::mutex> lk(this->detMu_);
        if (this->detection_) this->detection_->abort();
        return ok("detect.abort", "\"aborted\":true");
    });

    // detect.results
    reg.registerMethod("detect.results", "Return detection peak RPMs per PWM", [&](const RpcRequest&) -> RpcResult {
        std::lock_guard<std::mutex> lk(this->detMu_);
        json arr = json::array();
        if (this->detection_) {
            auto v = this->detection_->results();
            for (size_t i = 0; i < v.size(); ++i) {
                json item;
                item["pwm"] = (i < hwmon_.pwms.size() ? hwmon_.pwms[i].path_pwm : std::string{});
                item["peak_rpm"] = v[i];
                arr.push_back(item);
            }
        }
        std::ostringstream body; body << "\"results\":" << arr.dump();
        return ok("detect.results", body.str());
    });

    // engine.enable
    reg.registerMethod("engine.enable", "Enable automatic control", [&](const RpcRequest&) -> RpcResult {
        this->engine_.enableControl(true);
        return ok("engine.enable", "\"control\":true");
    });

    // engine.disable
    reg.registerMethod("engine.disable", "Disable automatic control", [&](const RpcRequest&) -> RpcResult {
        this->engine_.enableControl(false);
        return ok("engine.disable", "\"control\":false");
    });

    // engine.status
    reg.registerMethod("engine.status", "Basic engine status", [&](const RpcRequest&) -> RpcResult {
        std::ostringstream body;
        body << "\"control\":" << (this->engine_.controlEnabled() ? "true" : "false")
             << ",\"pwms\":" << this->hwmon_.pwms.size()
             << ",\"fans\":" << this->hwmon_.fans.size()
             << ",\"temps\":" << this->hwmon_.temps.size();
        return ok("engine.status", body.str());
    });

    // engine.reset
    reg.registerMethod("engine.reset", "Disable control and clear profile", [&](const RpcRequest&) -> RpcResult {
        this->engine_.enableControl(false);
        Profile empty;
        this->engine_.applyProfile(empty);
        return ok("engine.reset", "\"reset\":true");
    });

    // hwmon.snapshot
    reg.registerMethod("hwmon.snapshot", "Counts of discovered devices", [&](const RpcRequest&) -> RpcResult {
        std::ostringstream body;
        body << "\"pwms\":" << this->hwmon_.pwms.size()
             << ",\"fans\":" << this->hwmon_.fans.size()
             << ",\"temps\":" << this->hwmon_.temps.size();
        return ok("hwmon.snapshot", body.str());
    });

    // list.sensor
    reg.registerMethod("list.sensor", "List temperature sensors", [&](const RpcRequest&) -> RpcResult {
        json arr = json::array();
        for (const auto& t : this->hwmon_.temps) {
            arr.push_back(t.label.empty() ? t.path_input : t.label);
        }
        std::ostringstream body; body << "\"sensors\":" << arr.dump();
        return ok("list.sensor", body.str());
    });

    // list.fan
    reg.registerMethod("list.fan", "List fan tach inputs", [&](const RpcRequest&) -> RpcResult {
        json arr = json::array();
        for (const auto& f : this->hwmon_.fans) arr.push_back(f.path_input);
        std::ostringstream body; body << "\"fans\":" << arr.dump();
        return ok("list.fan", body.str());
    });

    // list.pwm
    reg.registerMethod("list.pwm", "List PWM outputs", [&](const RpcRequest&) -> RpcResult {
        json arr = json::array();
        for (const auto& p : this->hwmon_.pwms) arr.push_back(p.path_pwm);
        std::ostringstream body; body << "\"pwms\":" << arr.dump();
        return ok("list.pwm", body.str());
    });

    // telemetry.json
    reg.registerMethod("telemetry.json", "Return current SHM JSON blob", [&](const RpcRequest&) -> RpcResult {
        std::string s;
        if (!this->engine_.getTelemetry(s)) return err("telemetry.json", -32001, "no telemetry");
        std::ostringstream body; body << "\"telemetry\":" << (s.empty() ? "null" : s);
        return ok("telemetry.json", body.str());
    });

    // daemon.update
    reg.registerMethod("daemon.update", "Check/download latest release; params:{download:bool,target:string,repo?:\"owner/repo\"}", [&](const RpcRequest& req) -> RpcResult {
        std::string owner = "meigrafd", repo = "LinuxFanControl";
        json p;
        try {
            p = json::parse(req.paramsJson.empty() ? "{}" : req.paramsJson);
            if (p.contains("repo") && p["repo"].is_string()) {
                auto s = p["repo"].get<std::string>(); auto k = s.find('/');
                if (k != std::string::npos) { owner = s.substr(0, k); repo = s.substr(k + 1); }
            }
        } catch (...) {}
        ReleaseInfo rel; std::string e;
        if (!UpdateChecker::fetchLatest(owner, repo, rel, e)) return err("daemon.update", -32020, e);
        int cmp = UpdateChecker::compareVersions(LFC_VERSION, rel.tag);
        json out;
        out["current"] = LFC_VERSION;
        out["latest"] = rel.tag;
        out["name"] = rel.name;
        out["html"] = rel.htmlUrl;
        out["updateAvailable"] = (cmp < 0);
        out["assets"] = json::array();
        for (auto& a : rel.assets) out["assets"].push_back({{"name", a.name}, {"url", a.url}, {"type", a.type}, {"size", a.size}});

        bool dl = p.value("download", false);
        if (dl) {
            std::string target = p.value("target", std::string());
            if (target.empty()) return err("daemon.update", -32602, "missing target");
            std::string url;
            for (const auto& a : rel.assets) {
                std::string n = a.name; for (auto& c : n) c = (char)tolower(c);
                if (n.find("linux") != std::string::npos && (n.find("x86_64") != std::string::npos || n.find("amd64") != std::string::npos)) { url = a.url; break; }
            }
            if (url.empty() && !rel.assets.empty()) url = rel.assets[0].url;
            if (url.empty()) return err("daemon.update", -32021, "no assets");
            if (!UpdateChecker::downloadToFile(url, target, e)) return err("daemon.update", -32022, e);
            out["downloaded"] = true;
            out["target"] = target;
        }

        std::ostringstream body; body << "\"info\":" << out.dump();
        return ok("daemon.update", body.str());
    });

    // daemon.restart (signal supervisor to restart; here just ack)
    reg.registerMethod("daemon.restart", "Request daemon restart", [&](const RpcRequest&) -> RpcResult {
        // Supervisor (systemd/service wrapper) should act on this; we just acknowledge.
        return ok("daemon.restart", "\"accepted\":true");
    });

    // daemon.shutdown
    reg.registerMethod("daemon.shutdown", "Shutdown daemon gracefully", [&](const RpcRequest&) -> RpcResult {
        this->running_.store(false);
        return ok("daemon.shutdown", "\"stopping\":true");
    });
}

RpcResult Daemon::dispatch(const std::string& method, const std::string& paramsJson) {
    if (!reg_) return {false, "{\"code\":-32601,\"message\":\"no registry\"}"};
    RpcRequest req{method, "", paramsJson};
    return reg_->call(req);
}

std::vector<CommandInfo> Daemon::listRpcCommands() const {
    if (!reg_) return {};
    return reg_->list();
}

} // namespace lfc
