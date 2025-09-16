/*
 * Linux Fan Control — Daemon entry (main)
 * - CLI parsing and JSON config bootstrap
 * - Defaults to ~/.config/LinuxFanControl/ (XDG aware)
 * - Update check / update via GitHub Releases
 * - Optional CLI override to select active profile (name-only or *.json)
 * (c) 2025 LinuxFanControl contributors
 */
#include <iostream>
#include <string>
#include <filesystem>
#include <csignal>
#include <atomic>
#include <cstdlib>

#include "Config.hpp"          // provides DaemonConfig + loadDaemonConfig/saveDaemonConfig
#include "Daemon.hpp"
#include "RpcHandlers.hpp"
#include "include/CommandRegistry.h"
#include "UpdateChecker.hpp"
#include "Version.hpp"

using lfc::Daemon;
using lfc::DaemonConfig;
using lfc::UpdateChecker;

struct CliOptions {
    bool debug{false};
    bool listCommands{false};
    bool foreground{false};
    bool checkUpdate{false};
    bool doUpdate{false};

    std::string updateTarget;
    std::string repoOwner{"meigrafd"};
    std::string repoName{"LinuxFanControl"};

    std::string configPath;
    std::string pidfile{"/run/lfcd.pid"};
    std::string logfile{"/tmp/daemon_lfc.log"};
    std::string bindHost{"127.0.0.1"};
    std::uint16_t bindPort{8777};
    std::string shmPath{"/dev/shm/lfc_telemetry"};
    std::string profilesDir;

    // Allow selecting the active profile via CLI (e.g. "Gaming" or "Gaming.json")
    std::string activeProfileOverride;
};

static std::atomic<bool> g_stop{false};

static std::filesystem::path xdg_config_home() {
    const char* xdg = std::getenv("XDG_CONFIG_HOME");
    if (xdg && *xdg) return std::filesystem::path(xdg);
    const char* home = std::getenv("HOME");
    if (home && *home) return std::filesystem::path(home) / ".config";
    return std::filesystem::path(".");
}

static void usage(const char* argv0) {
    std::cout <<
        "LinuxFanControl Daemon (lfcd) v" << LFCD_VERSION << "\n"
        "Usage: " << argv0 << " [options]\n"
        "  --debug                   Enable debug logging\n"
        "  --foreground              Do not daemonize\n"
        "  --cmds                    List RPC commands and exit\n"
        "  --config PATH             Config file path (JSON)\n"
        "  --pidfile PATH            PID file path\n"
        "  --logfile PATH            Log file path\n"
        "  --host HOST               Bind host\n"
        "  --port PORT               Bind port\n"
        "  --shm PATH                SHM path\n"
        "  --profiles DIR            Profiles directory\n"
        "  --profile NAME[.json]     Set active profile (name-only stored)\n"
        "  --check-update            Check GitHub latest release and exit\n"
        "  --update                  Download latest release asset and exit\n"
        "  --update-target PATH      Where to save the downloaded binary\n"
        "  --repo OWNER/REPO         Override GitHub repo (default meigrafd/LinuxFanControl)\n";
}

// Strip ".json" (case-insensitive) to keep name-only in config.
static std::string to_profile_name(std::string s) {
    if (s.size() >= 5) {
        std::string tail = s.substr(s.size() - 5);
        for (char& c : tail) c = static_cast<char>(std::tolower(c));
        if (tail == ".json") s.erase(s.size() - 5);
    }
    return s;
}

static CliOptions parse_cli(int argc, char** argv) {
    CliOptions opt;

    auto base = xdg_config_home() / "LinuxFanControl";
    opt.configPath  = (base / "daemon.json").string();
    opt.profilesDir = (base / "profiles").string();

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto want = [&](const char* k) { return a.rfind(k, 0) == 0; };
        auto val  = [&](std::string& dst) { if (i + 1 < argc) dst = argv[++i]; };

        if (a == "-h" || a == "--help") { usage(argv[0]); std::exit(0); }
        else if (a == "--debug") opt.debug = true;
        else if (a == "--cmds") opt.listCommands = true;
        else if (a == "--foreground") opt.foreground = true;
        else if (a == "--check-update") opt.checkUpdate = true;
        else if (a == "--update") opt.doUpdate = true;
        else if (want("--update-target")) val(opt.updateTarget);
        else if (want("--repo")) {
            std::string repostr; val(repostr);
            auto slash = repostr.find('/');
            if (slash != std::string::npos) {
                opt.repoOwner = repostr.substr(0, slash);
                opt.repoName  = repostr.substr(slash + 1);
            }
        }
        else if (want("--config") || want("-c")) val(opt.configPath);
        else if (want("--pidfile")) val(opt.pidfile);
        else if (want("--logfile")) val(opt.logfile);
        else if (want("--host"))    val(opt.bindHost);
        else if (want("--port"))    { std::string s; val(s); opt.bindPort = static_cast<std::uint16_t>(std::stoi(s)); }
        else if (want("--shm"))     val(opt.shmPath);
        else if (want("--profiles"))val(opt.profilesDir);
        else if (want("--profile")) val(opt.activeProfileOverride);
    }
    return opt;
}

static void on_signal(int) { g_stop.store(true, std::memory_order_relaxed); }

static void ensure_dirs(const std::filesystem::path& cfgPath, const std::filesystem::path& profilesDir) {
    try {
        std::filesystem::create_directories(cfgPath.parent_path());
        std::filesystem::create_directories(profilesDir);
    } catch (...) {}
}

static int run_update_flow(const CliOptions& cli) {
    lfc::ReleaseInfo rel;
    std::string err;
    if (!UpdateChecker::fetchLatest(cli.repoOwner, cli.repoName, rel, err)) {
        std::cerr << "update: fetch failed: " << err << "\n";
        return 3;
    }

    std::cout << "current=" << LFCD_VERSION << " latest=" << rel.tag << " name=" << rel.name << "\n";
    int cmp = UpdateChecker::compareVersions(LFCD_VERSION, rel.tag);
    if (cmp >= 0) {
        std::cout << "up-to-date\n";
        return 0;
    }

    if (!cli.doUpdate) {
        std::cout << "update available: " << rel.tag << "\n";
        for (const auto& a : rel.assets) {
            std::cout << "asset: " << a.name << " (" << a.size << " bytes) -> " << a.url << "\n";
        }
        return 0;
    }

    if (cli.updateTarget.empty()) {
        std::cerr << "update: --update-target PATH required to download\n";
        return 4;
    }

    std::string url;
    for (const auto& a : rel.assets) {
        std::string n = a.name;
        for (auto& c : n) c = (char)tolower(c);
        if (n.find("linux") != std::string::npos &&
            (n.find("x86_64") != std::string::npos || n.find("amd64") != std::string::npos)) {
            url = a.url;
            break;
        }
    }
    if (url.empty() && !rel.assets.empty()) url = rel.assets[0].url;

    if (url.empty()) {
        std::cerr << "update: no downloadable assets found\n";
        return 5;
    }

    std::string err2;
    if (!UpdateChecker::downloadToFile(url, cli.updateTarget, err2)) {
        std::cerr << "update: download failed: " << err2 << "\n";
        return 6;
    }

    std::cout << "downloaded to " << cli.updateTarget << "\n";
    return 0;
}

int main(int argc, char** argv) {
    auto cli = parse_cli(argc, argv);

    // Early exits with zero side effects (no config IO, no hwmon init)
    if (cli.checkUpdate || cli.doUpdate) {
        return run_update_flow(cli);
    }
    if (cli.listCommands) {
        // List RPC commands without touching config/filesystem.
        auto reg = std::make_unique<lfc::CommandRegistry>();
        lfc::Daemon dummy;                         // not initialized; no IO
        lfc::BindDaemonRpcCommands(dummy, *reg);   // register names/help only
        for (const auto& cmd : reg->list()) {
            std::cout << cmd.name << "  —  " << cmd.help << "\n";
        }
        return 0;
    }

    ensure_dirs(cli.configPath, cli.profilesDir);

    // Load or create daemon config (flat DaemonConfig)
    DaemonConfig cfg;
    {
        std::string err;
        cfg = lfc::loadDaemonConfig(&err);   // API has no path parameter
        if (!err.empty()) {
            // Initialize defaults when file did not exist or parse failed.
            cfg.logfile      = cli.logfile;
            cfg.debug        = cli.debug;

            cfg.pidfile      = cli.pidfile;

            cfg.host         = cli.bindHost;
            cfg.port         = static_cast<int>(cli.bindPort);

            cfg.shmPath      = cli.shmPath;

            cfg.profilesDir  = cli.profilesDir;
            cfg.profileName  = "Default";     // store name only

            // Engine defaults (overridable via RPC/config.set)
            if (cfg.tickMs <= 0)      cfg.tickMs = 25;      // 5..1000
            if (cfg.deltaC <= 0.0)    cfg.deltaC = 0.5;     // 0.0..10.0
            if (cfg.forceTickMs <= 0) cfg.forceTickMs = 1000; // 100..10000

            std::string serr;
            (void)lfc::saveDaemonConfig(cfg, cli.configPath, &serr);
        } else {
            // Ensure profilesDir has a sensible default if empty
            if (cfg.profilesDir.empty()) {
                cfg.profilesDir = cli.profilesDir;
            }
        }

        // Apply CLI active-profile override (persist name-only)
        if (!cli.activeProfileOverride.empty()) {
            cfg.profileName = to_profile_name(cli.activeProfileOverride);
            std::string serr;
            (void)lfc::saveDaemonConfig(cfg, cli.configPath, &serr);
        }
    }

    Daemon daemon;
    if (!daemon.init(cfg, cli.debug, cli.configPath, cli.foreground)) {
        std::cerr << "[fatal] daemon init failed\n";
        return 2;
    }

    std::signal(SIGINT,  on_signal);
    std::signal(SIGTERM, on_signal);

    daemon.runLoop();
    daemon.shutdown();
    return 0;
}
