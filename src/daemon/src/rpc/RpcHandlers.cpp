/*
 * Linux Fan Control — RPC binder (thin aggregator)
 * (c) 2025 LinuxFanControl contributors
 */
#include "include/Daemon.hpp"
#include "include/CommandRegistry.hpp"
#include "include/Log.hpp"

#include "rpc/ImportJobs.hpp"

namespace lfc {
// Forward declarations for all RPC binders
void BindRpcCore(Daemon&, CommandRegistry&);
void BindRpcConfig(Daemon&, CommandRegistry&);
void BindRpcHwmonList(Daemon&, CommandRegistry&);
void BindRpcEngine(Daemon&, CommandRegistry&);
void BindRpcDetect(Daemon&, CommandRegistry&);
void BindRpcProfile(Daemon&, CommandRegistry&);
void BindRpcTelemetry(Daemon&, CommandRegistry&);
void BindRpcDaemon(Daemon&, CommandRegistry&);
void BindRpcImportAs(Daemon&, CommandRegistry&);
void BindRpcImportStatus(Daemon&, CommandRegistry&);
void BindRpcImportCommit(Daemon&, CommandRegistry&);
void BindRpcImportJobs(Daemon&, CommandRegistry&);
void BindRpcImportCancel(Daemon&, CommandRegistry&);

// Keep this file tiny; all handlers live in src/rpc/*
void BindDaemonRpcCommands(Daemon& self, CommandRegistry& reg) {
    LOG_TRACE("rpc: binding commands");

    // Core/system
    BindRpcCore(self, reg);
    BindRpcDaemon(self, reg);
    BindRpcConfig(self, reg);

    // Engine / control
    BindRpcEngine(self, reg);

    // Hwmon inventory listing
    BindRpcHwmonList(self, reg);

    // Detection
    BindRpcDetect(self, reg);

    // Import pipeline (original endpoints only)
    BindRpcImportAs(self, reg);
    BindRpcImportJobs(self, reg);
    BindRpcImportStatus(self, reg);
    BindRpcImportCancel(self, reg);
    BindRpcImportCommit(self, reg);

    // Profiles & telemetry
    BindRpcProfile(self, reg);
    BindRpcTelemetry(self, reg);

    LOG_DEBUG("rpc: binding complete");
}

} // namespace lfc
