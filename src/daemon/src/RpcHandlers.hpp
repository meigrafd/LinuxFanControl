/*
 * Linux Fan Control — RPC handlers (header)
 * - Registers all JSON-RPC methods for the daemon
 * (c) 2025 LinuxFanControl contributors
 */
#pragma once
#include <string>

namespace lfc {
class Daemon;
class CommandRegistry;

void BindDaemonRpcCommands(Daemon& self, CommandRegistry& reg);

} // namespace lfc
