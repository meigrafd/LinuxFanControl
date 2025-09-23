// src/daemon/src/Config.cpp
/*
 * Linux Fan Control — Configuration model and API (implementation)
 * (c) 2025 LinuxFanControl contributors
 *
 * Scope limited to requested tasks (1,2,3,6) and to restoring legacy API overloads:
 *  - Preserve key order on save (ordered_json; update in place, append new keys).
 *  - Default logfile: prefer /var/log/lfc/daemon_lfc.log, fallback /tmp/daemon_lfc.log.
 *  - Keep XDG helpers (xdg_state_home etc.) intact.
 *  - Do NOT move vendor mapping logic here.
 *  - Provide legacy overloads:
 *      DaemonConfig loadDaemonConfig(std::string* err);
 *      DaemonConfig loadDaemonConfig(const std::string& path, std::string* err);
 *      bool saveDaemonConfig(const DaemonConfig& c, const std::string& path, std::string* err);
 */

#include "include/Config.hpp"
#include "include/Utils.hpp"

#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <vector>
#include <string>
#include <system_error>
#include <stdexcept>
#include <cstdlib>
#include <unistd.h>

namespace fs = std::filesystem;
using nlohmann::json;
using nlohmann::ordered_json;

namespace lfc {

/* ----------------------------------------------------------------------------
 * helpers (env / fs)
 * ----------------------------------------------------------------------------*/

static inline std::string getenv_str(const char* key) {
    const char* c = std::getenv(key);
    return c ? std::string(c) : std::string();
}

static inline std::string xdg_home_fallback(const char* var, const char* defSuffix) {
    auto v = getenv_str(var);
    if (!v.empty()) return v;
    auto home = getenv_str("HOME");
    if (!home.empty()) return (fs::path(home) / defSuffix).string();
    return {};
}

static inline std::string xdg_config_home() {
    return xdg_home_fallback("XDG_CONFIG_HOME", ".config");
}

static inline std::string xdg_state_home() {
    return xdg_home_fallback("XDG_STATE_HOME", ".local/state");
}

static inline std::string xdg_runtime_dir() {
    return getenv_str("XDG_RUNTIME_DIR");
}

static inline std::string join(const std::string& a, const std::string& b) {
    if (a.empty()) return b;
    return (fs::path(a) / b).string();
}

static bool parent_writable(const std::string& path) {
    std::error_code ec;
    fs::path p(path);
    fs::path dir = p.parent_path().empty() ? fs::path(".") : p.parent_path();
    fs::create_directories(dir, ec); // best effort
    if (ec) return false;

    auto probe = (dir / ".lfc_write_testXXXXXX").string();
    std::vector<char> tmp(probe.begin(), probe.end());
    tmp.push_back('\0');
    int fd = mkstemp(tmp.data());
    if (fd < 0) return false;
    ::close(fd);
    fs::remove(tmp.data(), ec);
    return true;
}

/* ----------------------------------------------------------------------------
 * json (de)serialization
 * ----------------------------------------------------------------------------*/

void to_json(json& j, const DaemonConfig& c) {
    // Fixed write order for new files; existing files preserve order via saveDaemonConfig().
    j = json{
        {"configFile",           c.configFile},
        {"profilesPath",         c.profilesPath},
        {"logfile",              c.logfile},
        {"shmPath",              c.shmPath},
        {"pidfile",              c.pidfile},
        {"host",                 c.host},
        {"port",                 c.port},
        {"tickMs",               c.tickMs},
        {"forceTickMs",          c.forceTickMs},
        {"deltaC",               c.deltaC},
        {"debug",                c.debug},
        {"profileName",          c.profileName},
        {"vendorMapPath",        c.vendorMapPath},
        {"vendorMapWatchMode",   c.vendorMapWatchMode},
        {"vendorMapThrottleMs",  c.vendorMapThrottleMs}
    };
}

void from_json(const nlohmann::json& j, DaemonConfig& c) {
    if (j.contains("host"))                 j.at("host").get_to(c.host);
    if (j.contains("port"))                 j.at("port").get_to(c.port);
    if (j.contains("tickMs"))               j.at("tickMs").get_to(c.tickMs);
    if (j.contains("forceTickMs"))          j.at("forceTickMs").get_to(c.forceTickMs);
    if (j.contains("deltaC"))               j.at("deltaC").get_to(c.deltaC);
    if (j.contains("pidfile"))              j.at("pidfile").get_to(c.pidfile);
    if (j.contains("logfile"))              j.at("logfile").get_to(c.logfile);
    if (j.contains("debug"))                j.at("debug").get_to(c.debug);
    if (j.contains("profileName"))          j.at("profileName").get_to(c.profileName);
    if (j.contains("profilesPath"))         j.at("profilesPath").get_to(c.profilesPath);
    if (j.contains("shmPath"))              j.at("shmPath").get_to(c.shmPath);
    if (j.contains("vendorMapPath"))        j.at("vendorMapPath").get_to(c.vendorMapPath);
    if (j.contains("vendorMapWatchMode"))   j.at("vendorMapWatchMode").get_to(c.vendorMapWatchMode);
    if (j.contains("vendorMapThrottleMs"))  j.at("vendorMapThrottleMs").get_to(c.vendorMapThrottleMs);
}

/* ----------------------------------------------------------------------------
 * public API
 * ----------------------------------------------------------------------------*/

DaemonConfig defaultConfig() {
    DaemonConfig c;

    const std::string cfgHome   = xdg_config_home();
    const std::string stateHome = xdg_state_home();
    const std::string runDir    = xdg_runtime_dir();
    (void)stateHome; (void)runDir; // kept for compatibility; not used for logfile default per (6)

    const std::string baseCfg   = cfgHome.empty()   ? std::string() : join(cfgHome, "LinuxFanControl");
    // const std::string baseState = stateHome.empty() ? std::string() : join(stateHome, "LinuxFanControl");

    c.configFile   = baseCfg.empty() ? "" : (fs::path(baseCfg) / "daemon.json").string();
    c.profilesPath = baseCfg.empty() ? "" : join(baseCfg, "profiles");

    // Requirement (6): default logfile policy: prefer /var/log/lfc/daemon_lfc.log, else /tmp/daemon_lfc.log
    const std::string logVar = "/var/log/lfc/daemon_lfc.log";
    const std::string logTmp = "/tmp/daemon_lfc.log";
    c.logfile = parent_writable(logVar) ? logVar : logTmp;

    // Prefer /run; fall back to /tmp if not writable.
    const std::string runPid = "/run/lfcd.pid";
    const std::string tmpPid = "/tmp/lfcd.pid";
    c.pidfile = parent_writable(runPid) ? runPid : tmpPid;

    // POSIX SHM default is a name (normalized by ShmTelemetry to "/lfc.telemetry").
    c.shmPath     = "lfc.telemetry";

    // No explicit vendor map by default; search defaults/env at runtime (VendorMapping subsystem).
    c.vendorMapPath.clear();
    c.vendorMapWatchMode = "mtime";
    c.vendorMapThrottleMs = 3000;

    return c;
}

/* Normalize paths (non-persistent) */
static void expandPaths_(DaemonConfig& c) {
    c.configFile     = lfc::util::expandUserPath(c.configFile);
    c.profilesPath   = lfc::util::expandUserPath(c.profilesPath);
    c.logfile        = lfc::util::expandUserPath(c.logfile);
    c.pidfile        = lfc::util::expandUserPath(c.pidfile);
    c.shmPath        = lfc::util::expandUserPath(c.shmPath);
    c.vendorMapPath  = lfc::util::expandUserPath(c.vendorMapPath);
}

/* Style A: explicit path I/O (throws) */
void loadDaemonConfig(const std::string& path, DaemonConfig& out) {
    // Start from sane defaults
    out = defaultConfig();

    // Resolve target path (explicit path wins; otherwise use default location)
    std::string p = !path.empty() ? lfc::util::expandUserPath(path)
                                  : lfc::util::expandUserPath(out.configFile);
    if (p.empty()) {
        throw std::runtime_error("No config path resolved (empty XDG_CONFIG_HOME/HOME?)");
    }

    std::error_code ec;
    if (!fs::exists(p, ec) || ec) {
        // File missing (or stat error) -> create directories and write defaults
        util::ensure_parent_dirs(p, &ec);
        if (ec) {
            throw std::runtime_error("Cannot create parent dirs for: " + p + " (" + ec.message() + ")");
        }

        std::string werr;
        if (!saveDaemonConfig(out, p, &werr)) {
            throw std::runtime_error("Config not found and cannot be created: " + p + " (" + werr + ")");
        }
        // Successfully created default config; keep 'out' as defaults and return
        expandPaths_(out);
        return;
    }

    // Load existing file and overlay onto defaults
    std::ifstream f(p);
    if (!f.is_open()) {
        throw std::runtime_error("Cannot open config: " + p);
    }

    // Accept both plain object and wrapped forms; preserve order info if present.
    ordered_json j = ordered_json::parse(f, nullptr, true);
    const ordered_json* jobj = &j;
    if (j.is_object() && j.contains("config") && j["config"].is_object()) {
        jobj = &j["config"];
    }

    DaemonConfig merged = out; // defaults
    from_json(*jobj, merged);  // overlay values from file
    out = std::move(merged);
    expandPaths_(out);
}

/* Style A: throwing save (pretty, preserves order) */
void saveDaemonConfig(const std::string& path, const DaemonConfig& c) {
    const std::string p = lfc::util::expandUserPath(path);

    std::error_code ec;
    util::ensure_parent_dirs(p, &ec);
    if (ec) {
        throw std::runtime_error("Cannot create parent dirs for: " + p + " (" + ec.message() + ")");
    }

    // Load existing file (if any) to preserve insertion order
    ordered_json existing = ordered_json::object();
    {
        std::ifstream in(p);
        if (in) {
            try { existing = ordered_json::parse(in, nullptr, true); }
            catch (...) { existing = ordered_json::object(); }
        }
    }

    // Merge: update existing keys in place, append new keys at tail
    json jNew; to_json(jNew, c);

    // If existing was wrapped { "config": {...}, "loaded": true }, unwrap for in-place editing
    ordered_json* conf = &existing;
    ordered_json wrapper;
    bool wrapped = false;
    if (existing.is_object() && existing.contains("config") && existing["config"].is_object()) {
        conf = &existing["config"];
        wrapped = true;
    }

    for (auto it = jNew.begin(); it != jNew.end(); ++it) {
        const std::string& k = it.key();
        if (conf->contains(k)) (*conf)[k] = *it; else (*conf)[k] = *it;
    }

    // If not wrapped, just write object; if wrapped, keep envelope with loaded=true
    ordered_json outj;
    if (wrapped) {
        existing["loaded"] = true;
        outj = existing;
    } else {
        outj = *conf;
    }

    std::ofstream f(p, std::ios::trunc);
    if (!f.is_open()) {
        throw std::runtime_error("Cannot write config: " + p);
    }
    f << outj.dump(2) << "\n";
}

/* ----------------------------------------------------------------------------
 * Style B: legacy/convenience APIs (used by various call sites)
 * ----------------------------------------------------------------------------*/

DaemonConfig loadDaemonConfig(std::string* err) {
    DaemonConfig cfg = defaultConfig();
    if (err) *err = {};
    try {
        if (!cfg.configFile.empty()) {
            loadDaemonConfig(cfg.configFile, cfg);
        }
    } catch (const std::exception& ex) {
        if (err) *err = ex.what();
    }
    return cfg;
}

DaemonConfig loadDaemonConfig(const std::string& path, std::string* err) {
    DaemonConfig cfg = defaultConfig();
    if (err) *err = {};
    try {
        loadDaemonConfig(path, cfg);
    } catch (const std::exception& ex) {
        if (err) *err = ex.what();
    }
    return cfg;
}

bool saveDaemonConfig(const DaemonConfig& c, const std::string& path, std::string* err) {
    if (err) *err = {};
    try {
        std::string target = !path.empty() ? path : c.configFile;
        if (target.empty()) {
            target = defaultConfig().configFile;
        }
        saveDaemonConfig(target, c);
        return true;
    } catch (const std::exception& ex) {
        if (err) *err = ex.what();
        return false;
    }
}

/* ----------------------------------------------------------------------------
 * Convenience namespace for callers expecting Config::*
 * ----------------------------------------------------------------------------*/
namespace Config {
std::string defaultConfigPath()   { return ::lfc::defaultConfig().configFile; }
std::string defaultProfilesPath() { return ::lfc::defaultConfig().profilesPath; }
std::string defaultLogfilePath()  { return ::lfc::defaultConfig().logfile; }
std::string defaultShmPath()      { return ::lfc::defaultConfig().shmPath; }
std::string defaultPidfilePath()  { return ::lfc::defaultConfig().pidfile; }
} // namespace Config

} // namespace lfc
