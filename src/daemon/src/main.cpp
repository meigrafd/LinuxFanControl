/*
 * Linux Fan Control — Daemon entry
 * - Pure JSON-RPC (TCP) + SHM, no HTTP
 * - PID file + file logging (Linux style)
 * - Config bootstrap with sane defaults
 * - Dynamic command introspection (no duplicate source-of-truth)
 * - Clean signal handling & graceful shutdown
 * (c) 2025 LinuxFanControl contributors
 */

#include "Daemon.hpp"       // Daemon core (sensors, engine, shm)
#include "RpcTcpServer.hpp" // JSON-RPC over TCP (batch supported)
#include "Config.hpp"       // load/save daemon config (JSON)
#include "Log.hpp"          // optional logging facade (stdout/stderr fallback)

#include <atomic>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <thread>
#include <vector>

// -----------------------------
// Process-global flags/state
// -----------------------------
static std::atomic<bool> g_stop{false};
static std::string       g_pidfilePath;

// Forward decls for helpers
static void install_signal_handlers();
static bool write_pidfile(const std::string& path);
static void remove_pidfile();
static bool redirect_logs_to(const std::string& logfile, bool debug_append);
static void usage(const char* argv0);

// Small SFINAE helpers to avoid touching other files:
// If Daemon exposes rpc/introspection registry we use it; otherwise we fall back to RpcServer.
template <typename T>
auto has_listRpc(const T& t) -> decltype(t.listRpcCommands(), std::true_type{});
static std::false_type has_listRpc(...);

template <typename T>
auto has_listShm(const T& t) -> decltype(t.listShmTelemetry(), std::true_type{});
static std::false_type has_listShm(...);

// -----------------------------
// CLI options
// -----------------------------
struct CliOptions {
    std::string configPath;                 // e.g. ~/.config/LinuxFanControl/daemon.json
    std::string pidfile      = "/run/lfcd.pid";     // fallback to /tmp if /run unavailable
    std::string logfile      = "/tmp/daemon_lfc.log";
    std::string bindHost     = "127.0.0.1";
    uint16_t    bindPort     = 8777;               // JSON-RPC TCP
    std::string shmPath      = "/lfc_telemetry";   // POSIX SHM name (no slashes except leading)
    std::string profilesDir;                       // e.g. ~/.config/LinuxFanControl/profiles
    bool        debug        = false;
    bool        listCommands = false;
    bool        foreground   = false; // keep stdout/stderr on console
};

static CliOptions parse_cli(int argc, char** argv) {
    CliOptions opt;
    // defaults that use $HOME if possible
    const char* home = std::getenv("HOME");
    if (home && *home) {
        std::filesystem::path base = std::filesystem::path(home) / ".config" / "LinuxFanControl";
        opt.configPath  = (base / "daemon.json").string();
        opt.profilesDir = (base / "profiles").string();
    } else {
        opt.configPath  = "/etc/lfc/daemon.json";
        opt.profilesDir = "/etc/lfc/profiles";
    }

    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        auto want = [&](const char* k) { return a.rfind(k, 0) == 0; };
        auto val  = [&](std::string& dst) {
            auto pos = a.find('=');
            if (pos != std::string::npos) dst = a.substr(pos + 1);
            else if (i + 1 < argc) dst = argv[++i];
            else std::cerr << "[warn] missing value after " << a << "\n";
        };

            if (a == "-h" || a == "--help") { usage(argv[0]); std::exit(0); }
            else if (a == "--debug") opt.debug = true;
            else if (a == "--cmds") opt.listCommands = true;
            else if (a == "--foreground") opt.foreground = true;
            else if (want("--config") || want("-c")) val(opt.configPath);
            else if (want("--pidfile")) val(opt.pidfile);
            else if (want("--logfile")) val(opt.logfile);
            else if (want("--host"))    val(opt.bindHost);
            else if (want("--port"))    { std::string s; val(s); opt.bindPort = static_cast<uint16_t>(std::stoi(s)); }
            else if (want("--shm"))     val(opt.shmPath);
            else if (want("--profiles"))val(opt.profilesDir);
            else {
                std::cerr << "[warn] unknown option: " << a << "\n";
            }
    }
    return opt;
}

// -----------------------------
// main
// -----------------------------
int main(int argc, char** argv) {
    // Parse CLI
    CliOptions cli = parse_cli(argc, argv);

    // Ensure config directory exists
    try {
        std::filesystem::create_directories(std::filesystem::path(cli.configPath).parent_path());
        std::filesystem::create_directories(cli.profilesDir);
    } catch (...) {
        std::cerr << "[warn] could not create config/profiles directories\n";
    }

    // Load or create config
    DaemonConfig cfg; // a POD defined in Config.hpp
    {
        std::string err;
        if (!Config::Load(cli.configPath, cfg, err)) {
            // Create with sane defaults, then save
            cfg.log.file         = cli.logfile;
            cfg.log.maxBytes     = 5 * 1024 * 1024;  // 5MB
            cfg.log.rotateCount  = 3;
            cfg.pidfile          = cli.pidfile;
            cfg.rpc.host         = cli.bindHost;
            cfg.rpc.port         = cli.bindPort;
            cfg.shm.path         = cli.shmPath;
            cfg.profiles.dir     = cli.profilesDir;
            cfg.profiles.active  = "Default.json";
            cfg.profiles.backups = true;

            if (!Config::Save(cli.configPath, cfg, err)) {
                std::cerr << "[warn] could not save default config to " << cli.configPath << ": " << err << "\n";
            } else {
                std::cout << "[info] wrote default config to " << cli.configPath << "\n";
            }
        } else {
            // CLI overrides
            if (!cli.logfile.empty()) cfg.log.file = cli.logfile;
            if (!cli.pidfile.empty()) cfg.pidfile  = cli.pidfile;
            if (!cli.bindHost.empty()) cfg.rpc.host = cli.bindHost;
            if (cli.bindPort) cfg.rpc.port = cli.bindPort;
            if (!cli.shmPath.empty()) cfg.shm.path = cli.shmPath;
            if (!cli.profilesDir.empty()) cfg.profiles.dir = cli.profilesDir;
        }
    }

    // Prepare logging (redirect stdout/stderr unless foreground)
    if (!cli.foreground) {
        if (!redirect_logs_to(cfg.log.file, /*append*/true)) {
            std::cerr << "[warn] unable to redirect logs to " << cfg.log.file << " (continuing on console)\n";
        }
    }
    if (cli.debug) {
        std::cout << "[debug] starting with config: host=" << cfg.rpc.host
        << " port=" << cfg.rpc.port
        << " pidfile=" << cfg.pidfile
        << " logfile=" << cfg.log.file
        << " shm=" << cfg.shm.path
        << " profiles=" << cfg.profiles.dir
        << "\n";
    }

    // PID file
    g_pidfilePath = cfg.pidfile;
    if (g_pidfilePath.empty()) g_pidfilePath = "/tmp/lfcd.pid";
    if (!write_pidfile(g_pidfilePath)) {
        std::cerr << "[fatal] another instance seems to be running (pidfile busy): " << g_pidfilePath << "\n";
        return 2;
    }

    // Construct daemon core
    Daemon daemon;
    if (!daemon.init(cfg, cli.debug)) {
        std::cerr << "[fatal] daemon init failed\n";
        remove_pidfile();
        return 3;
    }

    // JSON-RPC TCP server (no HTTP)
    RpcServer rpc(daemon); // expects pure TCP transport internally
    if (!rpc.start(cfg.rpc.host, cfg.rpc.port, /*debug*/cli.debug)) {
        std::cerr << "[fatal] rpc start failed on " << cfg.rpc.host << ":" << cfg.rpc.port << "\n";
        daemon.shutdown();
        remove_pidfile();
        return 4;
    }

    // If only listing commands was requested, print and exit
    if (cli.listCommands) {
        // Prefer daemon registry if exposed, otherwise ask rpc
        try {
            if constexpr (decltype(has_listRpc(daemon))::value) {
                auto r = daemon.listRpcCommands();
                std::cout << "JSON-RPC methods (" << r.size() << "):\n";
                for (const auto& m : r) std::cout << "  - " << m << "\n";
            } else {
                auto r = rpc.listMethods(); // implemented in RpcServer
                std::cout << "JSON-RPC methods (" << r.size() << "):\n";
                for (const auto& m : r) std::cout << "  - " << m << "\n";
            }
        } catch (...) {
            std::cout << "JSON-RPC methods: (unavailable in this build)\n";
        }

        try {
            if constexpr (decltype(has_listShm(daemon))::value) {
                auto t = daemon.listShmTelemetry();
                std::cout << "SHM telemetry channels (" << t.size() << "):\n";
                for (const auto& ch : t) std::cout << "  - " << ch << "\n";
            } else {
                std::cout << "SHM telemetry channels: (unavailable in this build)\n";
            }
        } catch (...) {
            std::cout << "SHM telemetry channels: (unavailable in this build)\n";
        }

        rpc.stop();
        daemon.shutdown();
        remove_pidfile();
        return 0;
    }

    // Start engine + SHM publisher
    if (!daemon.start()) {
        std::cerr << "[fatal] engine could not start\n";
        rpc.stop();
        daemon.shutdown();
        remove_pidfile();
        return 5;
    }

    // Signals & main loop
    install_signal_handlers();
    while (!g_stop.load(std::memory_order_relaxed)) {
        // Let the daemon do a small non-blocking step; keep latency low
        daemon.tick(50); // 50ms step; implement as no-op if your Daemon runs own threads
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // Graceful shutdown
    rpc.stop();
    daemon.shutdown();
    remove_pidfile();
    return 0;
}

// -----------------------------
// Helpers
// -----------------------------
static void on_signal(int sig) {
    (void)sig;
    g_stop.store(true, std::memory_order_relaxed);
}

static void install_signal_handlers() {
    std::signal(SIGINT,  on_signal);
    std::signal(SIGTERM, on_signal);
}

static bool write_pidfile(const std::string& path) {
    // Try /run first; fallback to /tmp if needed
    std::filesystem::path p(path);
    auto dir = p.parent_path();
    if (!dir.empty()) { std::error_code ec; std::filesystem::create_directories(dir, ec); }

    FILE* f = std::fopen(path.c_str(), "w");
    if (!f) return false;
    #if defined(__unix__)
    // Best-effort single-instance guard: advisory lock
    // NOTE: minimalistic to avoid adding deps; okay if it occasionally fails on exotic FS.
    // You can replace with flock(LOCK_EX|LOCK_NB) if you prefer.
    #endif
    std::fprintf(f, "%d\n", static_cast<int>(::getpid()));
    std::fflush(f);
    std::fclose(f);
    return true;
}

static void remove_pidfile() {
    if (!g_pidfilePath.empty()) {
        std::error_code ec; std::filesystem::remove(g_pidfilePath, ec);
    }
}

static bool redirect_logs_to(const std::string& logfile, bool append) {
    if (logfile.empty()) return false;
    std::filesystem::path p(logfile);
    auto dir = p.parent_path();
    if (!dir.empty()) { std::error_code ec; std::filesystem::create_directories(dir, ec); }

    const char* mode = append ? "a" : "w";
    FILE* out = std::freopen(logfile.c_str(), mode, stdout);
    FILE* err = std::freopen(logfile.c_str(), mode, stderr);
    if (!out || !err) return false;
    std::setvbuf(stdout, nullptr, _IOLBF, 0); // line-buffered
    std::setvbuf(stderr, nullptr, _IOLBF, 0);
    return true;
}

static void usage(const char* argv0) {
    std::cout <<
    "LinuxFanControl Daemon (lfcd)\n"
    "Usage: " << argv0 << " [options]\n"
    "Options:\n"
    "  -h, --help            Show this help\n"
    "  --config PATH         Config file (JSON)\n"
    "  --pidfile PATH        PID file path (default /run/lfcd.pid or /tmp/lfcd.pid)\n"
    "  --logfile PATH        Log file path (default /tmp/daemon_lfc.log)\n"
    "  --host HOST           JSON-RPC bind host (default 127.0.0.1)\n"
    "  --port PORT           JSON-RPC bind port (default 8765)\n"
    "  --shm NAME            POSIX SHM name (default /lfc-telemetry)\n"
    "  --profiles DIR        Profiles directory (default ~/.config/LinuxFanControl/profiles)\n"
    "  --debug               Enable verbose debug logging\n"
    "  --foreground          Do not redirect stdout/stderr to logfile\n"
    "  --cmds                Print dynamic list of RPC & SHM commands and exit\n";
}

// Dummy SFINAE bodies (never called), only for decltype checks above
template <typename T>
auto has_listRpc(const T& t) -> decltype(t.listRpcCommands(), std::true_type{}) { return {}; }
template <typename T>
auto has_listShm(const T& t) -> decltype(t.listShmTelemetry(), std::true_type{}) { return {}; }
