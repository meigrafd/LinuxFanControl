/*
 * Linux Fan Control (lfcd) - Daemon implementation
 * (c) 2025 meigrafd & contributors - MIT License
 *
 * This file intentionally does NOT contain a test main().
 * The only main() lives in src/daemon/src/main.cpp.
 */

#include "Daemon.h"
#include <iostream>

Daemon::Daemon(bool debug)
: debug_(debug) {
    // Keep construction light; no I/O here.
}

Daemon::~Daemon() = default;

bool Daemon::init() {
    if (debug_) {
        std::cerr << "[daemon] init()\n";
    }
    // TODO: register JSON-RPC handlers here (listSensors, listPwms, detectCalibrate, ...)
    // Example:
    // server_.on("listChannels", [](const nlohmann::json&) { return nlohmann::json::array(); });

    return true;
}

int Daemon::run() {
    if (debug_) {
        std::cerr << "[daemon] run() -> entering RPC loop\n";
    }
    // Blocks reading JSON lines from stdin and writing responses to stdout.
    return server_.run();
}

void Daemon::requestStop() {
    if (debug_) {
        std::cerr << "[daemon] requestStop()\n";
    }
    // Add server stop logic here if/when available, e.g. server_.stop();
}
