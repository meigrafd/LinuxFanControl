/*
 * Linux Fan Control — RPC Import API (prototypes)
 * (c) 2025 LinuxFanControl contributors
 */
#pragma once
#include "include/Daemon.hpp"
#include "include/CommandRegistry.hpp"

namespace lfc {
    void BindRpcImport(Daemon& self, CommandRegistry& reg);
    void BindRpcImportStatus(Daemon& self, CommandRegistry& reg);
    void BindRpcImportCommit(Daemon& self, CommandRegistry& reg);
} // namespace lfc
