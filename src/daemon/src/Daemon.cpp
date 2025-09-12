/*
 * Linux Fan Control (lfcd) - Daemon implementation
 * This file intentionally does NOT contain a test main().
 * (c) 2025 meigrafd & contributors - MIT License
 */

#include "Daemon.h"
#include <iostream>

// Keep includes minimal here; the concrete subsystem wiring happens elsewhere.
// If Daemon.h requires additional types, include their headers in Daemon.h.
Daemon::Daemon(bool debug)
: debug_(debug) {
    // Nothing heavy here; just remember the debug flag.
}

Daemon::~Daemon() = default;

bool Daemon::init() {
    // Initialize subsystems as needed (Hwmon probing, engine prep, etc.).
    // Keep it lightweight to avoid side effects during unit tests.
    if (debug_) {
        std::cerr << "[daemon] init()\n";
    }
    return true;
}

int Daemon::run() {
    // Handlers should be registered during construction or init() in your server layer.
    // This call blocks on the JSON-RPC server loop reading stdin/writing stdout.
    if (debug_) {
        std::cerr << "[daemon] run() -> entering RPC loop\n";
    }
    return server_.run();
}

void Daemon::requestStop() {
    // If your RpcServer supports cooperative stop, call it here.
    // Otherwise this is a no-op placeholder for future use.
    if (debug_) {
        std::cerr << "[daemon] requestStop()\n";
    }
    // server_.stop(); // implement if/when available
}
