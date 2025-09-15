/*
 * Linux Fan Control — Daemon entry (main)
 * - CLI parsing and config bootstrap (INI)
 * - Foreground/daemon mode toggle
 * - Starts daemon, installs signal handlers
 * (c) 2025 LinuxFanControl contributors
 */
#include <iostream>
#include <string>
#include <filesystem>
#include <csignal>
#include <atomic>
#include <chrono>
#include <thread>

#include "Config.hpp"
#include "Daemon.hpp"

using lfc::Daemon;
using lfc::DaemonConfig;
using lfc::Config;

struct CliOptions {
    bool debug{false};
    bool listCommands{false};
    bool foreground{false};
    std::string configPath;
    std::string pidfile{"/run/lfcd.pid"};
    std::string logfile{"/tmp/daemon_lfc.log"};
    std::string bindHost{"127.0.0.1"};
    std::uint16_t bindPort{8777};
    std::string shmPath{"/dev/shm/lfc_telemetry"};
    std::string profilesDir;
};

static std::atomic<bool> g_stop{false};
static std::string g_pidfilePath;

static void usage(const char* argv0) {
    std::cout <<
    "LinuxFanControl Daemon (lfcd)\n"
    "Usage: " << argv0 << " [options]\n"
    "  --debug               Enable debug logging\n"
    "  --foreground          Do not daemonize\n"
    "  --cmds                List RPC commands and exit\n"
    "  --config PATH         Config file path\n"
    "  --pidfile PATH        PID file path\n"
    "  --logfile PATH        Log file path\n"
    "  --host HOST           Bind host\n"
    "  --port PORT           Bind port\n"
    "  --shm PATH            SHM path\n"
    "  --profiles DIR        Profiles directory\n";
}

static CliOptions parse_cli(int argc, char** argv) {
    CliOptions opt;
    const char* home = std::getenv("HOME");
    if (home && *home) {
        std::filesystem::path base = std::filesystem::path(home) / ".config" / "LinuxFanControl";
        opt.configPath  = (base / "daemon.ini").string();
        opt.profilesDir = (base / "profiles").string();
    } else {
        opt.configPath  = "/etc/lfc/daemon.ini";
        opt.profilesDir = "/etc/lfc/profiles";
    }

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto want = [&](const char* k) { return a.rfind(k, 0) == 0; };
        auto val  = [&](std::string& dst) {
            if (i + 1 < argc) dst = argv[++i];
        };

            if (a == "-h" || a == "--help") { usage(argv[0]); std::exit(0); }
            else if (a == "--debug") opt.debug = true;
            else if (a == "--cmds") opt.listCommands = true;
            else if (a == "--foreground") opt.foreground = true;
            else if (want("--config") || want("-c")) val(opt.configPath);
            else if (want("--pidfile")) val(opt.pidfile);
            else if (want("--logfile")) val(opt.logfile);
            else if (want("--host"))    val(opt.bindHost);
            else if (want("--port"))    { std::string s; val(s); opt.bindPort = static_cast<std::uint16_t>(std::stoi(s)); }
            else if (want("--shm"))     val(opt.shmPath);
            else if (want("--profiles"))val(opt.profilesDir);
    }
    return opt;
}

static void on_signal(int) { g_stop.store(true, std::memory_order_relaxed); }

int main(int argc, char** argv) {
    auto cli = parse_cli(argc, argv);

    try {
        std::filesystem::create_directories(std::filesystem::path(cli.configPath).parent_path());
        std::filesystem::create_directories(cli.profilesDir);
    } catch (...) {}

    DaemonConfig cfg;
    {
        std::string err;
        if (!Config::Load(cli.configPath, cfg, err)) {
            cfg.log.file         = cli.logfile;
            cfg.log.maxBytes     = 5 * 1024 * 1024;
            cfg.log.rotateCount  = 3;
            cfg.log.debug        = cli.debug;
            cfg.pidFile          = cli.pidfile;
            cfg.rpc.host         = cli.bindHost;
            cfg.rpc.port         = cli.bindPort;
            cfg.shm.path         = cli.shmPath;
            cfg.profiles.dir     = cli.profilesDir;
            cfg.profiles.active  = "Default.json";
            cfg.profiles.backups = true;
            Config::Save(cli.configPath, cfg, err);
        }
    }

    if (cli.debug) {
        std::cout << "[debug] starting with config: host=" << cfg.rpc.host
        << " port=" << cfg.rpc.port
        << " log=" << cfg.log.file
        << " shm=" << cfg.shm.path << "\n";
    }

    g_pidfilePath = cfg.pidFile;

    Daemon daemon;
    if (!daemon.init(cfg, cli.debug)) {
        std::cerr << "[fatal] daemon init failed\n";
        return 2;
    }

    std::signal(SIGINT,  on_signal);
    std::signal(SIGTERM, on_signal);

    daemon.runLoop();
    daemon.shutdown();
    return 0;
}
