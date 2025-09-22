/*
 * Linux Fan Control — Daemon entry (main)
 * (c) 2025 LinuxFanControl contributors
 */

#include <atomic>
#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <system_error>

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <nlohmann/json.hpp>

#include "include/Version.hpp"
#include "include/Config.hpp"
#include "include/Daemon.hpp"
#include "rpc/RpcHandlers.hpp"
#include "include/CommandRegistry.hpp"
#include "include/UpdateChecker.hpp"
#include "include/Log.hpp"

namespace fs = std::filesystem;
using nlohmann::json;
using lfc::Daemon;
using lfc::DaemonConfig;
using lfc::ReleaseAsset;
using lfc::ReleaseInfo;
using lfc::UpdateChecker;

static std::atomic<bool> gStop{false};
static void sig_handler(int) { gStop.store(true); }

static void usage(const char* exe) {
    std::cout <<
        "LinuxFanControl daemon (lfcd) " << LFCD_VERSION << "\n"
        "Usage: " << exe << " [options]\n"
        "Options:\n"
        "  --config PATH         Path to daemon.json (default: ~/.config/LinuxFanControl/daemon.json)\n"
        "  --profile NAME        Profile name to load (default: Default)\n"
        "  --profiles DIR        Directory with profiles (default: ~/.config/LinuxFanControl/profiles)\n"
        "  --pidfile PATH        PID file path (default: /tmp/lfcd.pid)\n"
        "  --logfile PATH        Log file path (default: /tmp/daemon_lfc.log)\n"
        "  --host IP             RPC host (default: 127.0.0.1)\n"
        "  --port N              RPC port (default: 8777)\n"
        "  --shm PATH            Shared memory path (default: /dev/shm/lfc.telemetry)\n"
        "  --tick-ms N           Engine tick in ms (default: 200)\n"
        "  --force-tick-ms N     Force engine tick in ms (default: 1000)\n"
        "  --delta-c V           Temperature delta threshold (default: 0.5)\n"
        "  --foreground          Do not daemonize; run in foreground\n"
        "  --debug               Verbose logging\n"
        "  --cmds                Print RPC command list and exit (no IO)\n"
        "  --check-update        Check GitHub releases for updates\n"
        "  --update              Download latest release asset\n"
        "  --update-target PATH  File to write when using --update\n"
        "  --repo owner/name     GitHub repo (default: meigrafd/LinuxFanControl)\n"
        "  -h,--help             Show this help\n";
}

static bool write_pidfile(const std::string& path, pid_t pid, std::string& err) {
    err.clear();
    int fd = ::open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) { err = "open pidfile failed"; return false; }
    char buf[64]; int n = std::snprintf(buf, sizeof(buf), "%d\n", (int)pid);
    if (::write(fd, buf, (size_t)n) != n) { ::close(fd); err = "write pidfile failed"; return false; }
    ::close(fd);
    return true;
}

static bool ensure_logdir(const fs::path& file) {
    std::error_code ec;
    fs::create_directories(file.parent_path(), ec);
    return !ec;
}

static bool daemonize(bool foreground, const std::string& logfile, const std::string& pidfile) {
    if (foreground) return true;

    pid_t pid = fork();
    if (pid < 0) return false;
    if (pid > 0) _exit(0); // parent exits

    if (setsid() < 0) return false;
    pid = fork();
    if (pid < 0) return false;
    if (pid > 0) _exit(0);

    umask(022);
    chdir("/");

    // Redirect stdio to logfile (fallback /tmp/daemon_lfc.log)
    std::string lf = logfile.empty() ? "/tmp/daemon_lfc.log" : logfile;
    fs::path lfp(lf);
    if (!ensure_logdir(lfp)) lf = "/tmp/daemon_lfc.log";

    int fd = ::open(lf.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd >= 0) {
        dup2(fd, STDOUT_FILENO);
        dup2(fd, STDERR_FILENO);
        int nullfd = ::open("/dev/null", O_RDONLY);
        if (nullfd >= 0) dup2(nullfd, STDIN_FILENO);
        if (nullfd >= 0) ::close(nullfd);
        ::close(fd);
    }

    // Write pidfile (fallback /tmp/lfcd.pid)
    std::string pf = pidfile.empty() ? "/tmp/lfcd.pid" : pidfile;
    std::string perr;
    if (!write_pidfile(pf, getpid(), perr)) {
        write_pidfile("/tmp/lfcd.pid", getpid(), perr);
    }

    return true;
}

// Pretty, aligned printing for `--cmds`
/*
static void print_commands_pretty(lfc::CommandRegistry& reg) {
    // The registry exposes a stable list() with name/help entries.
    const auto entries = reg.list();

    // Determine a reasonable column width [24..40]
    size_t width = 0;
    for (const auto& e : entries) if (e.name.size() > width) width = e.name.size();
    if (width < 24) width = 24;
    if (width > 40) width = 40;

    // Print sorted by command name
    std::vector<std::pair<std::string,std::string>> items;
    items.reserve(entries.size());
    for (const auto& e : entries) items.emplace_back(e.name, e.help);
    std::sort(items.begin(), items.end(), [](const auto& a, const auto& b){ return a.first < b.first; });

    for (const auto& it : items) {
        std::printf("%-*s %s\n", int(width), it.first.c_str(), it.second.c_str());
    }
}
*/
// pretty-print available commands (uses CommandRegistry::listJson)
static void print_commands_pretty(lfc::CommandRegistry& reg) {
    const nlohmann::json arr = reg.listJson(); // [{ "name": "...", "help": "..." }, ...]
    std::vector<std::pair<std::string,std::string>> entries;
    entries.reserve(arr.size());

    for (const auto& it : arr) {
        if (!it.is_object()) continue;
        const std::string name = it.value("name", "");
        const std::string help = it.value("help", "");
        if (!name.empty()) entries.emplace_back(name, help);
    }

    std::sort(entries.begin(), entries.end(),
              [](const auto& a, const auto& b){ return a.first < b.first; });

    fprintf(stdout, "Available RPC commands (%zu):\n", entries.size());
    for (const auto& [name, help] : entries) {
        fprintf(stdout, "  %-28s  %s\n", name.c_str(), help.c_str());
    }
}

static void install_signals() {
    std::signal(SIGINT,  sig_handler);
    std::signal(SIGTERM, sig_handler);
#ifdef SIGHUP
    std::signal(SIGHUP,  sig_handler);
#endif
}

int main(int argc, char** argv) {
    std::string cfgPath;
    std::string repo = "meigrafd/LinuxFanControl";
    bool foreground = false;
    bool debug = false;
    bool listCmds = false;
    bool doCheckUpdate = false;
    bool doUpdate = false;
    std::string updateTarget;

    // Parse CLI first (no filesystem/config yet)
    for (int i=1; i<argc; ++i) {
        std::string a = argv[i];
        auto next = [&](const char* what)->std::string {
            if (i+1>=argc) { std::cerr << "missing value for " << what << "\n"; std::exit(2); }
            return argv[++i];
        };
        if (a=="--config") cfgPath = next(a.c_str());
        else if (a=="--profile") { next(a.c_str()); }
        else if (a=="--profiles") { next(a.c_str()); }
        else if (a=="--pidfile")  { next(a.c_str()); }
        else if (a=="--logfile")  { next(a.c_str()); }
        else if (a=="--host")     { next(a.c_str()); }
        else if (a=="--port")     { next(a.c_str()); }
        else if (a=="--shm")      { next(a.c_str()); }
        else if (a=="--tick-ms")  { next(a.c_str()); }
        else if (a=="--force-tick-ms") { next(a.c_str()); }
        else if (a=="--delta-c")  { next(a.c_str()); }
        else if (a=="--foreground") foreground = true;
        else if (a=="--debug") debug = true;
        else if (a=="--cmds") listCmds = true;
        else if (a=="--check-update") doCheckUpdate = true;
        else if (a=="--update") doUpdate = true;
        else if (a=="--update-target") updateTarget = next(a.c_str());
        else if (a=="--repo") repo = next(a.c_str());
        else if (a=="-h" || a=="--help") { usage(argv[0]); return 0; }
        else {
            std::cerr << "unknown arg: " << a << "\n";
            usage(argv[0]);
            return 2;
        }
    }

    // --cmds: list RPC commands WITHOUT touching config/filesystem.
    if (listCmds) {
        // Build a temporary registry and bind handlers to list commands deterministically.
        lfc::CommandRegistry reg;
        lfc::Daemon dummy; // only to satisfy bind signatures
        lfc::BindDaemonRpcCommands(dummy, reg);
        print_commands_pretty(reg);
        return 0;
    }

    // Load base config (may be overridden by --config)
    std::string loadErr;
    DaemonConfig cfg = lfc::loadDaemonConfig(&loadErr);
    if (!cfgPath.empty()) {
        DaemonConfig c2 = lfc::loadDaemonConfig(cfgPath, &loadErr);
        if (loadErr.empty()) cfg = c2;
        else std::cerr << "[warn] load config: " << loadErr << "\n";
    }

    if (debug) cfg.debug = true;

    install_signals();

    // Update check/download (no daemon init)
    if (doCheckUpdate || doUpdate) {
        ReleaseInfo info;
        std::string err;
        auto pos = repo.find('/');
        std::string owner = (pos==std::string::npos) ? "meigrafd" : repo.substr(0,pos);
        std::string name  = (pos==std::string::npos) ? "LinuxFanControl" : repo.substr(pos+1);

        if (!UpdateChecker::fetchLatest(owner, name, info, err)) {
            LOG_ERROR("[update] failed: %s", err.c_str());
            return 1;
        }

        std::cout << "[update] latest tag: " << info.tag << "  name: " << info.name << "\n";
        std::cout << "[update] url: " << info.htmlUrl << "\n";
        if (doUpdate) {
            if (updateTarget.empty()) {
                std::cerr << "[update] need --update-target PATH\n";
                return 2;
            }
            if (info.assets.empty()) {
                std::cerr << "[update] no assets in release\n";
                return 3;
            }
            std::string aurl = info.assets[0].url;
            std::string e;
            if (!UpdateChecker::downloadToFile(aurl, updateTarget, e)) {
                LOG_ERROR("[update] download failed: %s", e.c_str());
                return 4;
            }
            std::cout << "[update] saved to " << updateTarget << "\n";
        }
        return 0;
    }

    // Daemonize (respecting explicit foreground)
    if (!daemonize(foreground, cfg.logfile, cfg.pidfile)) {
        LOG_ERROR("daemonize failed");
        return 1;
    }

    // Initialize logger AFTER daemonize so file descriptors are correct.
    {
        const std::string logPath = cfg.logfile.empty() ? "/tmp/daemon_lfc.log" : cfg.logfile;
        const bool mirror = foreground || debug;
        lfc::Logger::instance().init(
            logPath,
            debug ? lfc::LogLevel::Debug : lfc::LogLevel::Info,
            mirror,
            nullptr
        );
        LOG_INFO("lfcd starting (version %s)", LFCD_VERSION);
    }

    // Create daemon instance and initialize from cfg
    Daemon daemon;
    daemon.setConfigPath(cfgPath.empty() ? lfc::Config::defaultConfigPath() : cfgPath);
    daemon.setProfilesDir(cfg.profilesDir);
    daemon.setActiveProfile(cfg.profileName);
    daemon.setRpcHost(cfg.host);
    daemon.setRpcPort(cfg.port);
    daemon.setShmPath(cfg.shmPath);
    daemon.setDebug(cfg.debug);
    daemon.setEngineTickMs(cfg.tickMs);
    daemon.setEngineForceTickMs(cfg.forceTickMs);
    daemon.setEngineDeltaC(cfg.deltaC);

    if (!daemon.init()) {
        LOG_ERROR("daemon init failed");
        return 2;
    }

    LOG_DEBUG("lfcd ready on %s:%d (profile=%s)", cfg.host.c_str(), cfg.port, cfg.profileName.c_str());

    // Register RPC commands
    lfc::BindDaemonRpcCommands(daemon, daemon.rpcRegistry());

    // Run loop in dedicated thread; main thread handles signals.
    std::thread loopThread([&]{
        daemon.runLoop();
    });

    // Wait for a stop signal
    while (!gStop.load(std::memory_order_relaxed)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    LOG_INFO("lfcd shutting down (signal received)");
    daemon.shutdown();

    if (loopThread.joinable()) loopThread.join();

    lfc::Logger::instance().shutdown();

    return 0;
}
