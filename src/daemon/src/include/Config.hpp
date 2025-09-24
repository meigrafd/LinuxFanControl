/*
 * Linux Fan Control — Daemon configuration (public interface)
 * (c) 2025 LinuxFanControl contributors
 *
 * NOTE:
 *  - This header exposes the DaemonConfig model and file APIs.
 *  - ENV fallbacks are applied in the .cpp (see loadDaemonConfig path).
 */
#pragma once
#include <string>
#include <vector>
#include <optional>
#include <nlohmann/json.hpp>

namespace lfc {

/* ----------------------------------------------------------------------------
 * Daemon configuration model
 * ----------------------------------------------------------------------------*/
struct DaemonConfig {
    // RPC server
    std::string host{"127.0.0.1"};
    int         port{8777};

    // Engine timing
    int         tickMs{50};
    int         forceTickMs{2000};
    double      deltaC{0.7};

    // GPU-Refresh (NVML/IGCL/AMDSMI)
    int         gpuRefreshMs{1000};
    // hwmon-Refresh/Housekeeping
    int         hwmonRefreshMs{500};

    // Files / paths
    std::string pidfile;        // default computed in defaultConfig()
    std::string logfile;        // default computed in defaultConfig()
    std::string configFile;     // XDG-based default daemon.json
    std::string profilesPath;   // XDG-based default profiles/

    // Telemetry (POSIX SHM name; normalized by ShmTelemetry)
    std::string shmPath{"lfc.telemetry"};

    // Vendor mapping file/watch settings
    std::string vendorMapPath;          // empty = search default
    std::string vendorMapWatchMode{"mtime"};
    int         vendorMapThrottleMs{3000};

    // Other flags
    bool        debug{false};

    // Optional: active profile name at boot
    std::string profileName;
};

// JSON (de)serialization
void to_json(nlohmann::json& j, const DaemonConfig& c);
void from_json(const nlohmann::json& j, DaemonConfig& c);

// Expand "~", "$VAR", "${VAR}" in a path
std::string expandUserPath(const std::string& in);

/* ----------------------------------------------------------------------------
 * Build platform defaults (XDG-aware)
 * ----------------------------------------------------------------------------*/
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
    std::string defaultProfilesPath();
    std::string defaultLogfilePath();
    std::string defaultShmPath();
    std::string defaultPidfilePath();
}

} // namespace lfc
