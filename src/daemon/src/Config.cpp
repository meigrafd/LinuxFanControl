/*
 * Linux Fan Control — Config (implementation)
 * - Uses nlohmann/json with 4-space pretty print
 * (c) 2025 LinuxFanControl contributors
 */
#include "Config.hpp"

#include <nlohmann/json.hpp>
#include <fstream>
#include <sstream>
#include <filesystem>

using nlohmann::json;

namespace lfc {

DaemonConfig Config::Defaults() {
    return DaemonConfig{};
}

static bool read_file(const std::string& path, std::string& out, std::string& err) {
    if (!std::filesystem::exists(path)) { err = "config not found"; return false; }
    std::ifstream f(path);
    if (!f) { err = "open failed"; return false; }
    std::ostringstream ss; ss << f.rdbuf();
    out = ss.str();
    return true;
}

bool Config::Load(const std::string& path, DaemonConfig& out, std::string& err) {
    std::string txt;
    if (!read_file(path, txt, err)) return false;

    json j;
    try {
        j = json::parse(txt);
    } catch (const std::exception& e) {
        err = std::string("parse failed: ") + e.what();
        return false;
    }

    DaemonConfig cfg = Defaults();

    if (j.contains("log") && j["log"].is_object()) {
        const auto& jl = j["log"];
        cfg.log.file        = jl.value("file",        cfg.log.file);
        cfg.log.maxBytes    = static_cast<std::size_t>(jl.value("maxBytes",   static_cast<std::uint64_t>(cfg.log.maxBytes)));
        cfg.log.rotateCount = jl.value("rotateCount", cfg.log.rotateCount);
        cfg.log.debug       = jl.value("debug",       cfg.log.debug);
    }

    if (j.contains("rpc") && j["rpc"].is_object()) {
        const auto& jr = j["rpc"];
        cfg.rpc.host = jr.value("host", cfg.rpc.host);
        cfg.rpc.port = jr.value("port", cfg.rpc.port);
    }

    if (j.contains("shm") && j["shm"].is_object()) {
        const auto& js = j["shm"];
        cfg.shm.path = js.value("path", cfg.shm.path);
    }

    if (j.contains("profiles") && j["profiles"].is_object()) {
        const auto& jp = j["profiles"];
        cfg.profiles.dir     = jp.value("dir",     cfg.profiles.dir);
        cfg.profiles.active  = jp.value("active",  cfg.profiles.active);
        cfg.profiles.backups = jp.value("backups", cfg.profiles.backups);
    }

    cfg.pidFile = j.value("pidFile", cfg.pidFile);

    out = cfg;
    return true;
}

bool Config::Save(const std::string& path, const DaemonConfig& in, std::string& err) {
    std::error_code ec;
    std::filesystem::create_directories(std::filesystem::path(path).parent_path(), ec);

    json j;
    j["log"] = {
        {"file",        in.log.file},
        {"maxBytes",    static_cast<std::uint64_t>(in.log.maxBytes)},
        {"rotateCount", in.log.rotateCount},
        {"debug",       in.log.debug}
    };
    j["rpc"] = {
        {"host", in.rpc.host},
        {"port", in.rpc.port}
    };
    j["shm"] = {
        {"path", in.shm.path}
    };
    j["profiles"] = {
        {"dir",     in.profiles.dir},
        {"active",  in.profiles.active},
        {"backups", in.profiles.backups}
    };
    j["pidFile"] = in.pidFile;

    std::ofstream f(path, std::ios::trunc);
    if (!f) { err = "open for write failed"; return false; }
    f << j.dump(4) << '\n';
    return static_cast<bool>(f);
}

} // namespace lfc
