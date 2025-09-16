/*
 * Linux Fan Control — main
 * - Initializes daemon and runs the main loop until terminated
 * - If no profile exists, keep running and wait for detection/import via RPC
 * (c) 2025 LinuxFanControl contributors
 */
#include "Daemon.hpp"
#include "Config.hpp"
#include "Log.hpp"

#include <csignal>
#include <filesystem>
#include <iostream>
#include <string>

using namespace lfc;
namespace fs = std::filesystem;

static Daemon* g_daemon = nullptr;

static std::string normalize_profile_name(const std::string& in) {
    if (in.empty()) return in;
    if (in.size() > 5 && in.substr(in.size() - 5) == ".json") return in;
    return in + ".json";
}

static void handle_signal(int sig) {
    (void)sig;
    if (g_daemon) g_daemon->requestStop();
}

int main(int, char**) {
    // Load daemon config (robust against type errors)
    std::string err;
    DaemonConfig cfg = loadDaemonConfig(&err);
    if (!err.empty()) {
        std::cerr << "[warn] config load: " << err << "\n";
    }

    // Initialize daemon
    Daemon d;
    g_daemon = &d;

    // Install signal handlers for clean shutdown
    std::signal(SIGINT,  handle_signal);
    std::signal(SIGTERM, handle_signal);
#ifdef SIGHUP
    std::signal(SIGHUP,  handle_signal);
#endif

    if (!d.init(cfg, cfg.debug, cfg.configFile, cfg.foreground)) {
        std::cerr << "[error] daemon init failed\n";
        return 1;
    }

    // Try to apply configured profile if present; otherwise keep running
    if (!cfg.profileName.empty()) {
        const std::string candidate =
            cfg.profilesDir + "/" + normalize_profile_name(cfg.profileName);
        if (fs::exists(candidate)) {
            (void)d.applyProfileFile(candidate);
        } else {
            LFC_LOGI("no profile present at %s — awaiting detection/import via RPC",
                     candidate.c_str());
        }
    } else {
        LFC_LOGI("no profile configured — awaiting detection/import via RPC");
    }

    LFC_LOGI("lfcd started on %s:%d (tickMs=%d, deltaC=%.2f, forceTickMs=%d)",
             cfg.host.c_str(), cfg.port, cfg.tickMs, cfg.deltaC, cfg.forceTickMs);

    // Run until signal
    d.runLoop();

    LFC_LOGI("lfcd stopped");
    return 0;
}
