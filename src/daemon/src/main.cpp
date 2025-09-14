/*
 * Linux Fan Control — Daemon entry
 * Provides CLI for pidfile/log/debug and command listing.
 * Starts HTTP JSON-RPC server with batch support at /rpc.
 * (c) 2025 LinuxFanControl contributors
 */

#include "Daemon.hpp"
#include "RpcServer.hpp"
#include <iostream>
#include <string>
#include <csignal>
#include <memory>

static Daemon* g_daemon = nullptr;
static std::unique_ptr<RpcServer> g_rpc;

static void onSig(int) {
    if (g_daemon) g_daemon->requestShutdown();
    if (g_rpc) g_rpc->stop();
}

static void usage(const char* argv0) {
    std::cout <<
    "Usage: " << argv0 << " [options]\n"
    "  --pidfile PATH      PID file path (default /tmp/lfcd.pid)\n"
    "  --logfile PATH      Log file path (default /tmp/daemon_lfc.log)\n"
    "  --debug             Verbose logging\n"
    "  --host HOST         Bind host (default 127.0.0.1)\n"
    "  --port N            Bind port (default 8765)\n"
    "  --cmds              Print RPC commands and exit\n"
    "  -h, --help          Show this help\n";
}

int main(int argc, char** argv) {
    Daemon::Options opt;
    bool listOnly = false;

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--pidfile" && i + 1 < argc) { opt.pidfile = argv[++i]; }
        else if (a == "--logfile" && i + 1 < argc) { opt.logfile = argv[++i]; }
        else if (a == "--debug") { opt.debug = true; }
        else if (a == "--host" && i + 1 < argc) { opt.bindHost = argv[++i]; }
        else if (a == "--port" && i + 1 < argc) { opt.bindPort = static_cast<uint16_t>(std::stoi(argv[++i])); }
        else if (a == "--cmds") { listOnly = true; }
        else if (a == "-h" || a == "--help") { usage(argv[0]); return 0; }
        else { std::cerr << "Unknown option: " << a << "\n"; usage(argv[0]); return 2; }
    }

    Daemon d;
    std::string err;
    if (!d.init(opt, err)) { std::cerr << "[fatal] " << err << "\n"; return 1; }

    if (listOnly) {
        auto cmds = d.listCommands();
        for (auto& c : cmds) std::cout << c.name << " - " << c.description << "\n";
        return 0;
    }

    g_daemon = &d;
    std::signal(SIGINT,  onSig);
    std::signal(SIGTERM, onSig);

    g_rpc = std::make_unique<RpcServer>(d, opt.bindHost, opt.bindPort, opt.debug);
    if (!g_rpc->start(err)) { std::cerr << "[fatal] rpc start failed: " << err << "\n"; return 1; }

    while (d.isRunning()) d.pumpOnce(50);
    g_rpc->stop();
    return 0;
}
