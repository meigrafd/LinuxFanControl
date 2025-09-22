/*
 * Linux Fan Control — Configuration model and API (header)
 * (c) 2025 LinuxFanControl contributors
 */
#pragma once

#include <string>
#include <optional>
#include <nlohmann/json.hpp>

namespace lfc {

/*
 * DaemonConfig — runtime configuration for the daemon.
 * Paths may contain "~", "$VAR" and "${VAR}" and are expanded at runtime.
 */
struct DaemonConfig {
    // Files / paths (expanded at runtime)
    std::string configFile;   // e.g. ~/.config/LinuxFanControl/daemon.json
    std::string profilesPath; // directory with profile JSON files
    std::string logfile;      // log file (rotated by Logger)
    std::string pidfile;      // pid file (e.g. /run/lfcd.pid or /tmp/lfcd.pid)

    // RPC endpoint
    std::string host{"127.0.0.1"};
    int         port{8777};

    // Engine behavior
    int     tickMs{500};
    int     forceTickMs{0};
    double  deltaC{0.5};
    bool    debug{false};

    // Active profile
    std::string profileName;

    // Telemetry shared memory name (POSIX SHM)
    // Use a simple name like "lfc.telemetry" (will become "/lfc.telemetry").
    std::string shmPath{"lfc.telemetry"};

    // Optional vendor mapping JSON (regex→vendor). If empty:
    // lookup will search default locations + LFC_VENDOR_MAP env.
    std::string vendorMapPath{};             // optional explicit path
    std::string vendorMapWatchMode{"mtime"}; // "mtime" or "inotify"
    int         vendorMapThrottleMs{3000};   // only used in mtime mode
};

// JSON (de)serialization
void to_json(nlohmann::json& j, const DaemonConfig& c);
void from_json(const nlohmann::json& j, DaemonConfig& c);

// Expand "~", "$VAR", "${VAR}" in a path
std::string expandUserPath(const std::string& in);

// Build platform defaults (XDG-aware)
DaemonConfig defaultConfig();

/* ----------------------------------------------------------------------------
 * File API (multiple styles to match existing call sites)
 * ----------------------------------------------------------------------------*/

// Style A: explicit path in/out (throws on error)
void loadDaemonConfig(const std::string& path, DaemonConfig& out);
void saveDaemonConfig(const std::string& path, const DaemonConfig& c);

// Style B: convenience/legacy APIs used by various call sites
DaemonConfig loadDaemonConfig(std::string* err);                                  // load defaults or default file; set err on failure
DaemonConfig loadDaemonConfig(const std::string& path, std::string* err);         // load explicit path; set err on failure
bool        saveDaemonConfig(const DaemonConfig& c, const std::string& path,
                             std::string* err);                                   // save to explicit path or c.configFile

/* ----------------------------------------------------------------------------
 * Namespace-style helpers kept for compatibility with existing code
 * ----------------------------------------------------------------------------*/
namespace Config {
    // Return fully-populated default configuration (same as ::lfc::defaultConfig()).
    DaemonConfig defaultConfig();

    // Individual default paths derived from environment/XDG rules.
    std::string defaultConfigPath();
    std::string defaultProfilesDir();
    std::string defaultLogfilePath();
    std::string defaultShmPath();
    std::string defaultPidfilePath();
}

} // namespace lfc
