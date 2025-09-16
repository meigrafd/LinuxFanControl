/*
 * Linux Fan Control — Daemon entry (main)
 * - CLI parsing and JSON config bootstrap
 * - Defaults to ~/.config/LinuxFanControl/ for config & profiles (XDG aware)
 * - Update check / update via GitHub Releases
 * - Optional CLI override to select active profile
 * (c) 2025 LinuxFanControl contributors
 */
#include <iostream>
#include <string>
#include <filesystem>
#include <csignal>
#include <atomic>
#include <cstdlib>

#include "Config.hpp"
#include "Daemon.hpp"
#include "RpcHandlers.hpp"
#include "include/CommandRegistry.h"
#include "UpdateChecker.hpp"
#include "Version.hpp"

using lfc::Daemon;
using lfc::DaemonConfig;
using lfc::Config;
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

    // New: allow selecting the active profile via CLI (e.g. "Gaming.json" or "Gaming")
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
        "LinuxFanControl Daemon (lfcd) v" << LFC_VERSION << "\n"
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
        "  --profile NAME            Set active profile (e.g. \"Gaming\" or \"Gaming.json\")\n"
        "  --check-update            Check GitHub latest release and exit\n"
        "  --update                  Download latest release asset and exit\n"
        "  --update-target PATH      Where to save the downloaded binary\n"
        "  --repo OWNER/REPO         Override GitHub repo (default meigrafd/LinuxFanControl)\n";
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

    std::cout << "current=" << LFC_VERSION << " latest=" << rel.tag << " name=" << rel.name << "\n";
    int cmp = UpdateChecker::compareVersions(LFC_VERSION, rel.tag);
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
        if (n.find("linux") != std::string::npos && (n.find("x86_64") != std::string::npos || n.find("amd64") != std::string::npos)) {
            url = a.url; break;
        }
    }
    if (url.empty() && !rel.assets.empty()) url = rel.assets[0].url;

    if (url.empty()) {
        std::cerr << "update: no downloadable assets found\n";
        return 5;
    }

    if (!UpdateChecker::downloadToFile(url, cli.updateTarget, err)) {
        std::cerr << "update: download failed: " << err << "\n";
        return 6;
    }

    std::cout << "downloaded to " << cli.updateTarget << "\n";
    return 0;
}

static std::string normalize_profile_name(const std::string& name) {
    if (name.empty()) return name;
    if (name.size() >= 5 && (name.rfind(".json") == name.size() - 5)) return name;
    return name + ".json";
}

int main(int argc, char** argv) {
    auto cli = parse_cli(argc, argv);

    if (cli.checkUpdate || cli.doUpdate) {
        return run_update_flow(cli);
    }

    if (cli.listCommands) {
        auto reg = std::make_unique<lfc::CommandRegistry>();
        lfc::Daemon dummy;
        lfc::BindDaemonRpcCommands(dummy, *reg);

        for (const auto& cmd : reg->list()) {
            std::cout << cmd.name << "  —  " << cmd.help << "\n";
        }
        return 0;
    }

    ensure_dirs(cli.configPath, cli.profilesDir);

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
        } else {
            if (cfg.profiles.dir.empty()) {
                cfg.profiles.dir = cli.profilesDir;
            }
        }

        // Apply CLI active-profile override (persist it for next runs)
        if (!cli.activeProfileOverride.empty()) {
            cfg.profiles.active = normalize_profile_name(cli.activeProfileOverride);
            std::string serr;
            (void)Config::Save(cli.configPath, cfg, serr);
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
