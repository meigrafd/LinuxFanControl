/*
 * Linux Fan Control — RPC binder (declaration)
 * (c) 2025 LinuxFanControl contributors
 */
#pragma once
#include "include/Daemon.hpp"
#include "include/CommandRegistry.hpp"

namespace lfc {
    // Registers all RPC commands into the given registry.
    void BindDaemonRpcCommands(Daemon& self, CommandRegistry& reg);
}
