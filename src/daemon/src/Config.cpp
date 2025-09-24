/*
 * Linux Fan Control — Daemon configuration (implementation)
 * (c) 2025 LinuxFanControl contributors
 *
 * Goals:
 *  - Keep existing behavior: defaults + JSON file are the source of truth.
 *  - Add ENV as a strict fallback layer (no renames, no redesigns):
 *      Defaults  ->  ENV  ->  daemon.json
 *  - XDG-aware defaults for config/profiles/log/pid.
 *  - Default logfile: prefer /var/log/lfc/daemon_lfc.log, fallback /tmp/daemon_lfc.log.
 *  - Keep namespace helpers for old call sites (Config::*).
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
#include <cctype>
#include <unistd.h>

namespace fs = std::filesystem;
using nlohmann::json;

namespace lfc {

/* ----------------------------------------------------------------------------
 * helpers (env / fs)
 * ----------------------------------------------------------------------------*/

static inline std::string getenv_str(const char* key) {
    const char* c = std::getenv(key);
    return c ? std::string(c) : std::string();
}
static inline int getenv_int(const char* key, int def) {
    const char* c = std::getenv(key);
    if (!c || !*c) return def;
    try { return std::stoi(c); } catch (...) { return def; }
}
static inline double getenv_double(const char* key, double def) {
    const char* c = std::getenv(key);
    if (!c || !*c) return def;
    try { return std::stod(c); } catch (...) { return def; }
}
static inline bool getenv_bool(const char* key, bool def) {
    const char* c = std::getenv(key);
    if (!c || !*c) return def;
    std::string v; v.assign(c);
    for (auto& ch : v) ch = (char)std::tolower((unsigned char)ch);
    if (v=="1"||v=="true"||v=="yes"||v=="on")  return true;
    if (v=="0"||v=="false"||v=="no" ||v=="off") return false;
    return def;
}

static inline std::string xdg_home_fallback(const char* var, const char* defSuffix) {
    auto v = getenv_str(var);
    if (!v.empty()) return v;
    auto home = getenv_str("HOME");
    if (!home.empty()) return (fs::path(home) / defSuffix).string();
    return {};
}

static inline std::string xdg_config_home() { return xdg_home_fallback("XDG_CONFIG_HOME", ".config"); }
static inline std::string xdg_state_home()  { return xdg_home_fallback("XDG_STATE_HOME",  ".local/state"); }
static inline std::string xdg_runtime_dir() { return getenv_str("XDG_RUNTIME_DIR"); }

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

void to_json(json& j, const lfc::DaemonConfig& c) {
    // Write out fields in a stable order
    j = json{
        {"host", c.host},
        {"port", c.port},

        {"tickMs", c.tickMs},
        {"forceTickMs", c.forceTickMs},
        {"deltaC", c.deltaC},
        {"gpuRefreshMs", c.gpuRefreshMs},
        {"hwmonRefreshMs", c.hwmonRefreshMs},

        {"pidfile", c.pidfile},
        {"logfile", c.logfile},

        {"debug", c.debug},

        {"profileName", c.profileName},
        {"profilesPath", c.profilesPath},

        {"shmPath", c.shmPath},

        {"vendorMapPath", c.vendorMapPath},
        {"vendorMapWatchMode", c.vendorMapWatchMode},
        {"vendorMapThrottleMs", c.vendorMapThrottleMs}
    };
}

void from_json(const nlohmann::json& j, DaemonConfig& c) {
    if (j.contains("host"))                 j.at("host").get_to(c.host);
    if (j.contains("port"))                 j.at("port").get_to(c.port);
    if (j.contains("tickMs"))               j.at("tickMs").get_to(c.tickMs);
    if (j.contains("forceTickMs"))          j.at("forceTickMs").get_to(c.forceTickMs);
    if (j.contains("deltaC"))               j.at("deltaC").get_to(c.deltaC);
    if (j.contains("gpuRefreshMs"))         j.at("gpuRefreshMs").get_to(c.gpuRefreshMs);
    if (j.contains("hwmonRefreshMs"))       j.at("hwmonRefreshMs").get_to(c.hwmonRefreshMs);
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
 * Defaults
 * ----------------------------------------------------------------------------*/

DaemonConfig defaultConfig() {
    DaemonConfig c;

    const std::string cfgHome   = xdg_config_home();
    const std::string stateHome = xdg_state_home();
    const std::string runDir    = xdg_runtime_dir();
    (void)stateHome; (void)runDir;

    const std::string baseCfg = cfgHome.empty() ? std::string() : join(cfgHome, "LinuxFanControl");

    c.configFile   = baseCfg.empty() ? "" : (fs::path(baseCfg) / "daemon.json").string();
    c.profilesPath = baseCfg.empty() ? "" : join(baseCfg, "profiles");

    // Logfile default: prefer /var/log, else /tmp
    const std::string logVar = "/var/log/lfc/daemon_lfc.log";
    const std::string logTmp = "/tmp/daemon_lfc.log";
    c.logfile = parent_writable(logVar) ? logVar : logTmp;

    // PID default: prefer /run, else /tmp
    const std::string runPid = "/run/lfcd.pid";
    const std::string tmpPid = "/tmp/lfcd.pid";
    c.pidfile = parent_writable(runPid) ? runPid : tmpPid;

    // SHM default is a name, normalized later by ShmTelemetry
    c.shmPath = "lfc.telemetry";

    // No explicit vendor map path by default
    c.vendorMapPath.clear();
    c.vendorMapWatchMode  = "mtime";
    c.vendorMapThrottleMs = 3000;

    return c;
}

/* ----------------------------------------------------------------------------
 * ENV overlay (fallback only)
 *  - Applies AFTER defaultConfig() and BEFORE reading daemon.json
 *  - Intent: let daemon.json override ENV if present.
 * ----------------------------------------------------------------------------*/
static void applyEnvFallbacks(DaemonConfig& c) {
    // Engine timing
    c.tickMs       = getenv_int   ("LFCD_TICK_MS",        c.tickMs);
    c.forceTickMs  = getenv_int   ("LFCD_FORCE_TICK_MS",  c.forceTickMs);
    c.deltaC       = getenv_double("LFCD_DELTA_C",        c.deltaC);

    // RPC
    {
        auto h = getenv_str("LFCD_HOST"); if (!h.empty()) c.host = h;
        c.port = getenv_int("LFCD_PORT", c.port);
    }

    // SHM
    {
        auto s = getenv_str("LFCD_SHM_PATH");
        if (s.empty()) s = getenv_str("LFC_SHM_PATH"); // accept historical alias
        if (!s.empty()) c.shmPath = s;
    }

    // Vendor mapping
    {
        auto p = getenv_str("LFC_VENDOR_MAP");          // absolute path, if set
        if (!p.empty()) c.vendorMapPath = p;

        auto wm = getenv_str("LFC_VENDOR_MAP_WATCH");   // "mtime" | "none" (kept as string)
        if (!wm.empty()) c.vendorMapWatchMode = wm;

        c.vendorMapThrottleMs = getenv_int("LFC_VENDOR_MAP_THROTTLE_MS", c.vendorMapThrottleMs);
    }

    // Log / PID (allow overrides if someone really wants that)
    {
        auto lf = getenv_str("LFCD_LOGFILE");  if (!lf.empty()) c.logfile  = lf;
        auto pf = getenv_str("LFCD_PIDFILE");  if (!pf.empty()) c.pidfile  = pf;
    }

    // Debug flag
    c.debug = getenv_bool("LFCD_DEBUG", c.debug);

    // Profiles
    {
        auto pp = getenv_str("LFCD_PROFILES_PATH"); if (!pp.empty()) c.profilesPath = pp;
        auto pn = getenv_str("LFCD_PROFILE_NAME");  if (!pn.empty()) c.profileName  = pn;
    }

    // Config path (only for helper calls that use default path; explicit --config still wins)
    {
        auto cp = getenv_str("LFCD_CONFIG_PATH");
        if (!cp.empty()) c.configFile = cp;
    }

    {
        c.gpuRefreshMs   = getenv_int("LFCD_GPU_REFRESH_MS",   c.gpuRefreshMs);
        c.hwmonRefreshMs = getenv_int("LFCD_HWMON_REFRESH_MS", c.hwmonRefreshMs);
    }
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

/* ----------------------------------------------------------------------------
 * Style A: explicit path I/O (throws)
 * ----------------------------------------------------------------------------*/
void loadDaemonConfig(const std::string& path, DaemonConfig& out) {
    // Start from sane defaults
    out = defaultConfig();

    // Apply ENV as fallback BEFORE reading JSON file (JSON overrides ENV).
    applyEnvFallbacks(out); // <— key addition (fallback layer)

    // Resolve target path (explicit path wins; otherwise use default location)
    std::string p = !path.empty() ? lfc::util::expandUserPath(path)
                                  : lfc::util::expandUserPath(out.configFile);
    if (p.empty()) {
        throw std::runtime_error("No config path resolved (empty XDG_CONFIG_HOME/HOME?)");
    }

    std::error_code ec;
    if (!fs::exists(p, ec) || ec) {
        // File missing (or stat error) -> create directories and write defaults (without persisting ENV)
        util::ensure_parent_dirs(p, &ec);
        if (ec) {
            throw std::runtime_error("Cannot create parent dirs for: " + p + " (" + ec.message() + ")");
        }
        json j; to_json(j, out);
        std::ofstream os(p);
        os << j.dump(2) << "\n";
        out.configFile = p;
        expandPaths_(out);
        return;
    }

    // Parse existing file and override any ENV-derived values
    json j = util::read_json_file(p);
    from_json(j, out);                // daemon.json wins over ENV
    out.configFile = p;

    expandPaths_(out);
}

void saveDaemonConfig(const std::string& path, const DaemonConfig& c) {
    std::error_code ec;
    const std::string target = lfc::util::expandUserPath(path);
    util::ensure_parent_dirs(target, &ec);
    if (ec) {
        throw std::runtime_error("Cannot create parent dirs for: " + target + " (" + ec.message() + ")");
    }
    json j; to_json(j, c);
    std::ofstream os(target);
    os << j.dump(2) << "\n";
}

/* ----------------------------------------------------------------------------
 * Style B: convenience/legacy APIs
 * ----------------------------------------------------------------------------*/
DaemonConfig loadDaemonConfig(std::string* err) {
    DaemonConfig cfg = defaultConfig();
    if (err) *err = {};
    try {
        // Apply ENV fallbacks then read default config file if present
        applyEnvFallbacks(cfg);
        loadDaemonConfig(cfg.configFile, cfg); // will reapply file; ENV remains fallback only
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
        const std::string target = !path.empty() ? path : c.configFile;
        if (target.empty()) {
            // fall back to a reasonable default location
            auto d = defaultConfig();
            saveDaemonConfig(d.configFile, c);
        } else {
            saveDaemonConfig(target, c);
        }
        return true;
    } catch (const std::exception& ex) {
        if (err) *err = ex.what();
        return false;
    }
}

/* ----------------------------------------------------------------------------
 * Namespace-style helpers
 * ----------------------------------------------------------------------------*/
namespace Config {
DaemonConfig defaultConfig()              { return ::lfc::defaultConfig(); }
std::string  defaultConfigPath()          { return ::lfc::defaultConfig().configFile; }
std::string  defaultProfilesPath()        { return ::lfc::defaultConfig().profilesPath; }
std::string  defaultLogfilePath()         { return ::lfc::defaultConfig().logfile; }
std::string  defaultShmPath()             { return ::lfc::defaultConfig().shmPath; }
std::string  defaultPidfilePath()         { return ::lfc::defaultConfig().pidfile; }
} // namespace Config

} // namespace lfc
