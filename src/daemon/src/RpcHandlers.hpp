/*
 * Linux Fan Control — RPC handlers (header)
 * Declares binding function for JSON-RPC methods.
 * (c) 2025 LinuxFanControl contributors
 */
#pragma once

#include <string>

// Forward declarations to keep this header light
namespace lfc {
class Daemon;
}

#include "include/CommandRegistry.h"  // defines CommandRegistry, CommandInfo, RpcRequest/Result

namespace lfc {

// Register all daemon RPC methods on the given registry.
void BindDaemonRpcCommands(Daemon& self, CommandRegistry& reg);

} // namespace lfc
