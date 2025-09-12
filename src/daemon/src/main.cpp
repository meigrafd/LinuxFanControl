/*
 * Linux Fan Control (lfcd) - Daemon entry point
 * (c) 2025 meigrafd & contributors - MIT License
 *
 * Notes:
 *  - Matches Daemon interface: init() + run()
 *  - Use --debug for verbose stderr logs
 */

#include "Daemon.h"

#include <csignal>
#include <iostream>
#include <string>

static volatile std::sig_atomic_t g_stop = 0;

static void handle_signal(int) {
    g_stop = 1;
    // If cooperative stop is added later, call d.requestStop() via a global pointer/singleton.
    // For now, the process will terminate by signal or stdin close.
}

int main(int argc, char** argv) {
    bool debug = false;

    // Simple flag parsing
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--debug") {
            debug = true;
        }
    }

    // Basic signal hooks (optional; current RpcServer::run() blocks on stdin)
    std::signal(SIGINT,  handle_signal);
    std::signal(SIGTERM, handle_signal);

    Daemon d(debug);
    if (!d.init()) {
        std::cerr << "[daemon] init() failed\n";
        return 1;
    }

    // Blocking JSON-RPC loop on stdin/stdout
    return d.run();
}
