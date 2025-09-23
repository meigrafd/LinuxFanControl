#pragma once
/*
 * Linux Fan Control — GPU Monitor (public interface + data model)
 * - Provides GPU discovery and metric refresh (temps, tach).
 * - Keeps a minimal but complete public API used across the daemon.
 */

#include <optional>
#include <string>
#include <vector>

namespace lfc {

/* Lightweight per-GPU snapshot for telemetry and control mapping. */
struct GpuSample {
    // identity
    std::string vendor;        // "AMD", "NVIDIA", "Intel", ...
    int         index{0};      // monotonic per-run index
    std::string name;          // pretty device name if available
    std::string pciBusId;      // "0000:03:00.0"
    std::string drmCard;       // "card0", "card1"
    std::string hwmonPath;     // base dir of hwmon for this GPU (may be empty)

    // capabilities (discovered)
    bool hasFanTach{false};
    bool hasFanPwm{false};

    // live metrics (refreshed)
    std::optional<int>    fanRpm;        // tachometer RPM
    std::optional<double> tempEdgeC;     // typical GPU edge temp
    std::optional<double> tempHotspotC;  // junction/hotspot
    std::optional<double> tempMemoryC;   // VRAM/memory temp
};

class GpuMonitor {
public:
    // One-shot discovery of GPUs. Fills 'out' with current devices.
    static void discover(std::vector<GpuSample>& out);

    // Refresh only the live metrics of already discovered GPUs.
    static void refreshMetrics(std::vector<GpuSample>& gpus);

    // Convenience: discover and return a copy (used by Daemon/FanControlImport).
    static std::vector<GpuSample> snapshot();

    // Map a temp "kind" to a hwmon file path under 'hwmonBase'.
    // kind accepts case-insensitive: "edge", "hotspot", "junction", "mem", "memory".
    static std::string resolveHwmonTempPath(const std::string& hwmonBase,
                                            const std::string& kind);

    // Optional vendor enrichers. Implementations may be no-ops if libs unavailable.
    static void enrichViaAMDSMI(std::vector<GpuSample>& vec);
    static void enrichViaNVML (std::vector<GpuSample>& vec);
    static void enrichViaIGCL (std::vector<GpuSample>& vec);
};

} // namespace lfc
