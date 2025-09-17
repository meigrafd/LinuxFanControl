/*
 * Linux Fan Control — Daemon Config (header)
 * - Daemon/runtime configuration (no profiles here)
 * - Env override via LFCD_* variables
 * (c) 2025 LinuxFanControl contributors
 */
#pragma once

#include <string>

namespace lfc {

struct DaemonConfig {
    // logging/runtime
    std::string logLevel        = "info";                   // LFCD_LOG_LEVEL
    bool        debug           = false;                    // LFCD_DEBUG
    bool        foreground      = false;                    // LFCD_FOREGROUND
    bool        cmds            = false;                    // LFCD_CMDS

    // networking
    std::string host            = "127.0.0.1";              // LFCD_HOST
    int         port            = 8777;                     // LFCD_PORT

    // paths
    std::string pidfile         = "/run/lfcd.pid";          // LFCD_PIDFILE
    std::string logfile         = "/var/log/lfc/daemon.log";// LFCD_LOGFILE
    std::string shmPath         = "/dev/shm/lfc_telemetry"; // LFCD_SHM

    // sensors backends
    std::string sysfsRoot       = "/sys/class/hwmon";       // LFCD_SYSFS_ROOT
    std::string sensorsBackend  = "auto";                   // LFCD_SENSORS_BACKEND (auto|sysfs|libsensors)

    // profiles (locations only)
    std::string profilesDir     = "~/.config/LinuxFanControl/profiles"; // LFCD_PROFILES_DIR
    std::string profileName     = "Default";                // LFCD_PROFILE

    // loop tuning
    int         tickMs          = 25;                       // LFCD_TICK_MS (5..1000)
    double      deltaC          = 0.5;                      // LFCD_DELTA_C (0.0..10.0)
    int         forceTickMs     = 2000;                     // LFCD_FORCE_TICK_MS (100..10000)

    // resolved config path
    std::string configFile      = "";                       // LFCD_CONFIG (path)
};

// load/save helpers
DaemonConfig loadDaemonConfig(const std::string& configPath, std::string* err = nullptr);
DaemonConfig loadDaemonConfig(std::string* err = nullptr);

bool saveDaemonConfig(const DaemonConfig& cfg, std::string* err = nullptr);
bool saveDaemonConfig(const DaemonConfig& cfg, const std::string& path, std::string* err);

// misc utils
bool        parseBool(const std::string& s);
std::string expandUserPath(const std::string& p);

} // namespace lfc
