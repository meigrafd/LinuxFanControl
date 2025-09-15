#pragma once
/*
 * Daemon configuration (loads/saves JSON file, creates defaults if missing).
 * (c) 2025 LinuxFanControl contributors
 */
#include <string>
#include <cstdint>

struct DaemonConfig {
    // logging
    std::string logfile        = "/var/log/lfc/daemon.log";
    std::string pidfile        = "/run/lfcd.pid";
    std::size_t log_size_bytes = 1 * 1024 * 1024; // 1 MiB
    int         log_rotate     = 5;               // keep N rotated files
    bool        debug          = false;

    // profiles
    std::string profiles_dir   = "/var/lib/lfc/profiles";
    std::string active_profile = "default";
    bool        profiles_backup= true;

    // JSON-RPC over TCP (no HTTP)
    std::string rpc_host       = "127.0.0.1";
    uint16_t    rpc_port       = 8777;

    // POSIX SHM
    std::string shm_path       = "/lfc_telemetry";

    // config file path (filled by loader)
    std::string _path;
};

namespace cfg {

    // Choose default config path (prefer /etc/lfc/daemon.json if writable else XDG)
    std::string defaultConfigPath();

    // Load or create default if missing (also ensures dirs).
    bool loadOrCreate(DaemonConfig& out, std::string customPath, std::string& err);

    // Save to existing path (overwrites). Creates backup if profiles_backup==true.
    bool save(const DaemonConfig& c, std::string& err);

    // Ensure runtime dirs exist (/var/log/lfc, /run/lfc, profiles_dir).
    bool ensureDirs(const DaemonConfig& c, std::string& err);

} // namespace cfg
