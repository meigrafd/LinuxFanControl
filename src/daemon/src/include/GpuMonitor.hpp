/*
 * Linux Fan Control — GPU monitor (optional external tools + hwmon)
 * (c) 2025 LinuxFanControl contributors
 */
#pragma once
#include <string>
#include <vector>

namespace lfc {

struct GpuSample {
    std::string vendor;       // AMD|NVIDIA|Intel|Unknown
    std::string name;         // product name or hwmon 'amdgpu' etc.
    std::string hwmonPath;    // /sys/class/hwmon/hwmonX (if known)
    std::string pciBusId;     // 0000:bb:dd.f  (if known)
    int         index{-1};    // GPU index if known (NVML)
    int         tempC{-1};    // temperature
    int         utilPct{-1};  // utilization percent if available
    int         memPct{-1};   // vram usage if available
    int         fanPct{-1};   // fan speed percent if available
    int         fanRpm{-1};   // fan RPM if available
    double      powerW{-1};   // optional
};

class GpuMonitor {
public:
    // One-shot inventory + initial metrics (deduplicated)
    static std::vector<GpuSample> snapshot();

    // Lightweight metrics refresh for an existing inventory (no re-discovery)
    static void refreshMetrics(std::vector<GpuSample>& inOut);

private:
    // Collectors (kernel/sysfs)
    static void collectFromHwmon(std::vector<GpuSample>& vec);
    static void collectFromDrm(std::vector<GpuSample>& vec);

    // Vendor enrich (update-or-merge, no duplicates)
#ifdef HAVE_NVML
    static void enrichNvml(std::vector<GpuSample>& vec);
#endif
#ifdef HAVE_AMDSMI
    static void enrichAmdSmi(std::vector<GpuSample>& vec);
#endif
#ifdef HAVE_IGCL
    static void enrichIntelIgcl(std::vector<GpuSample>& vec);
#endif

    // Helpers
    static std::string toLower(std::string v);
    static std::string pciFromDrmNode(const std::string& cardNode);
    static std::string normBusId(std::string bus);
    static int  findMatch(const std::vector<GpuSample>& vec,
                          const std::string& pci, const std::string& hwmon,
                          const std::string& vendor, const std::string& name);
    static void mergeInto(GpuSample& dst, const GpuSample& src);
};

} // namespace lfc
