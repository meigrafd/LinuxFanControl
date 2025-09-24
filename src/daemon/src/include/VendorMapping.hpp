/*
 * Linux Fan Control — Vendor/Chip/PCI mapping helpers
 * (c) 2025 LinuxFanControl contributors
 */
#pragma once
#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <optional>
#include <mutex>
#include <chrono>

namespace lfc {

class VendorMapping {
public:
    // Watch strategy for the JSON mapping file.
    enum class WatchMode { None, MTime, Inotify };

    // Singleton access.
    static VendorMapping& instance();

    // Configure external mapping path (e.g. ~/.config/LinuxFanControl/vendorMapping.json).
    // Takes effect immediately (loads file if present).
    void setOverridePath(const std::string& path);

    // Configure file watching behaviour. For MTime, changes are picked up on demand
    // when calling query methods (throttled by throttleMs).
    void setWatchMode(WatchMode mode, int throttleMs);

    // -------- Lookup API used across the project (Hwmon + others) --------

    // Return pretty vendor name for a hwmon chip name, falling back to the chip itself.
    std::string vendorForChipName(const std::string& chip);

    // Convenience overloads historically used in the codebase.
    std::string vendorFor(const std::string& chip)            { return vendorForChipName(chip); }
    std::string vendorFor(std::string_view chip)              { return vendorForChipName(std::string(chip)); }

    // Return alias list for a chip token (e.g. "nct6799d" -> ["nct6799","nct6799d","nuvoton","nct67"]).
    std::vector<std::string> chipAliasesFor(const std::string& chip);

    // ---- PCI helpers (used by GPU enrichment, but allgemein nutzbar) ----
    // Pretty name for Subsystem-Vendor-ID (sv). Applies aliases if configured.
    std::string pciVendorName(uint16_t subsystemVendorId) const;

    // Pretty board/model name for (sv, sd) if known; empty if unknown.
    std::string boardForSubsystem(uint16_t subsystemVendorId,
                                  uint16_t subsystemDeviceId) const;

    // ---- New GPU helpers (non-breaking) ------------------------------------
    // Returns canonical vendor: "NVIDIA" | "AMD" | "Intel" | "Unknown"
    std::string gpuCanonicalVendor(std::string_view s) const;

    // Return a comma-separated alias list for logging.
    std::string aliasesJoinForLog(const std::string& chip) const;

    // Parse identifiers from FanControl exports and return vendor/kind.
    std::pair<std::string, std::string> gpuVendorAndKindFromIdentifier(const std::string& identifier) const;

private:
    VendorMapping();
    ~VendorMapping() = default;

    struct Data {
        std::unordered_map<std::string, std::string>              chipVendor;          // chip -> pretty vendor
        std::unordered_map<std::string, std::vector<std::string>> chipAliases;         // chip -> aliases

        // --- Optional PCI maps from vendorMapping.json ---
        std::unordered_map<uint16_t, std::string> pciVendorPretty;      // sv -> pretty vendor
        std::unordered_map<std::string, std::string> pciVendorAliases;  // "raw" -> "alias"
        std::unordered_map<uint32_t, std::string> pciSubsystemOverrides; // (sv<<16)|sd -> model
    };

    // Load JSON from given path if exists; returns true on success (no locking inside).
    bool loadFromPath(const std::string& path) const;

    void ensureLoadedLocked() const;         // requires mtx_ held by caller
    void pollReloadIfNeededLocked() const;   // requires mtx_ held by caller

    // Utilities implemented in .cpp
    static std::string joinCsv(const std::vector<std::string>& v, const char* sep);
    static bool containsI(std::string_view hay, std::string_view needle);

    // pci.ids loader + caches
    static void ensurePciDbLoaded();
    static std::string pciIdsVendorName(uint16_t sv);
    static std::string pciIdsSubsystemName(uint16_t sv, uint16_t sd);

private:
    // Mutable for lazy reload in const methods
    mutable std::mutex mtx_;
    mutable Data       data_;

    mutable std::string overridePath_;
    mutable std::string defaultPath_; // ~/.config/LinuxFanControl/vendorMapping.json (computed)

    mutable WatchMode watchMode_{WatchMode::MTime};
    mutable int       watchThrottleMs_{3000};

    // For MTime strategy
    mutable std::chrono::steady_clock::time_point lastPoll_{};
    mutable long long  lastSeenMtime_{0};
    mutable std::string loadedPath_;

    // pci.ids caches (process-wide)
    static std::once_flag s_pciOnce_;
    static std::mutex     s_pciMx_;
    static bool           s_pciLoaded_;
    static std::unordered_map<uint16_t, std::string> s_pciVendorNames_;
    static std::unordered_map<uint32_t, std::string> s_pciSubsysNames_;
};

} // namespace lfc
