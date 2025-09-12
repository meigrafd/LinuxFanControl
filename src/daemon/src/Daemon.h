#pragma once
/*
 * Linux Fan Control (lfcd) - Daemon interface
 * (c) 2025 meigrafd & contributors - MIT License
 *
 * Notes:
 *  - This header declares a single Daemon class used by main.cpp.
 *  - It matches Daemon.cpp (constructor with optional debug flag, run/init/stop).
 *  - RpcServer is used for JSON-RPC 2.0 stdin/stdout loop.
 */

#include <cstdint>
#include <string>
#include "RpcServer.h"

class Daemon {
public:
    // Constructor with optional debug flag; also acts as a default constructor.
    explicit Daemon(bool debug = false);
    ~Daemon();

    // Initialize subsystems (lightweight; no long-running work here).
    bool init();

    // Enter the JSON-RPC processing loop (blocking until stdin closes).
    int run();

    // Cooperative stop hook (no-op unless server gains stop support).
    void requestStop();

private:
    bool debug_ = false;
    RpcServer server_;
};
