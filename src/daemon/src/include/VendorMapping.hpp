/*
 * Linux Fan Control — Vendor mapping (header)
 * (c) 2025 LinuxFanControl contributors
 */
#pragma once
#include <filesystem>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace lfc {

class VendorMapping {
public:
    enum class WatchMode { None, MTime, Inotify };

    static VendorMapping& instance();

    void setSearchPaths(const std::vector<std::filesystem::path>& paths);
    void setOverridePath(const std::filesystem::path& path);

    // Overloads kept for compatibility
    void setWatchMode(WatchMode mode);
    void setWatchMode(WatchMode mode, int throttleMs);

    void setThrottleMs(int ms);

    void ensureLoaded();
    void tryReloadIfChanged();

    // ---- Existing public API (unchanged) -----------------------------------
    std::string vendorForChipName(const std::string& chip) const;
    std::string vendorFor(const std::string& chip)            { return vendorForChipName(chip); }
    std::string vendorFor(std::string_view chip)              { return vendorForChipName(std::string(chip)); }
    std::vector<std::string> chipAliasesFor(const std::string& profileToken) const;

    // ---- New GPU helpers (non-breaking) ------------------------------------
    // Returns canonical vendor: "NVIDIA" | "AMD" | "Intel" | "Unknown"
    std::string gpuCanonicalVendor(std::string_view s) const;

    // Returns canonical temp kind: "Edge" | "Hotspot" | "Memory" | "Unknown"
    std::string gpuCanonicalTempKind(std::string_view s) const;

    // Parse identifiers like:
    //   "AMDSMI/AMD Radeon RX 7900 XT/0/temp/Hotspot"
    //   "NVML/NVIDIA GeForce RTX 4090/1/temp/GPU"
    //   "IGCL/Intel Arc A770/0/temp/Memory"
    // → {"AMD"|"NVIDIA"|"Intel"|"Unknown", "Edge"|"Hotspot"|"Memory"|"Unknown"}
    std::pair<std::string,std::string>
    gpuVendorAndKindFromIdentifier(std::string_view identifier) const;

private:
    friend class VendorMappingImpl;
    VendorMapping() = default;
};

} // namespace lfc
