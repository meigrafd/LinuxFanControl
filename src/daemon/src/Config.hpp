/*
 * Linux Fan Control — Config (header)
 * - JSON-based daemon configuration
 * - Minimal schema with sane defaults
 * (c) 2025 LinuxFanControl contributors
 */
#pragma once
#include <string>
#include <cstddef>

namespace lfc {

    struct DaemonConfig {
        struct Log {
            std::string file{"/tmp/daemon_lfc.log"};
            std::size_t maxBytes{5 * 1024 * 1024}; // 5 MB
            int         rotateCount{3};
            bool        debug{false};
        } log;

        struct Rpc {
            std::string host{"127.0.0.1"};
            int         port{8777};
        } rpc;

        struct Shm {
            std::string path{"/dev/shm/lfc_telemetry"};
        } shm;

        struct Profiles {
            std::string dir{"/var/lib/lfc/profiles"};
            std::string active{"Default.json"};
            bool        backups{true};
        } profiles;

        std::string pidFile{"/run/lfcd.pid"};
    };

    struct Config {
        // Returns a DaemonConfig with all default values
        static DaemonConfig Defaults();

        // Loads config from JSON file at 'path' into 'out'; returns false on error and sets 'err'
        static bool Load(const std::string& path, DaemonConfig& out, std::string& err);

        // Saves config to JSON file at 'path'; returns false on error and sets 'err'
        static bool Save(const std::string& path, const DaemonConfig& in, std::string& err);
    };

} // namespace lfc
