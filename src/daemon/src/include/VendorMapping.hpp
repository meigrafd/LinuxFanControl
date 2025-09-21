/*
 * Linux Fan Control — Vendor mapping (runtime JSON for hwmon chip vendors)
 * (c) 2025 LinuxFanControl contributors
 */
#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <mutex>
#include <shared_mutex>
#include <optional>
#include <filesystem>
#include <chrono>

namespace lfc {

/*
 * Loads vendor mapping rules from JSON at runtime, so new vendors can be added
 * without recompiling. Matching is case-insensitive substring match.
 *
 * Search order for the JSON file (first hit wins), unless LFCD_VENDOR_MAP is set:
 *   1) $LFCD_VENDOR_MAP
 *   2) ~/.config/LinuxFanControl/vendorMapping.json
 *   3) /etc/linuxfancontrol/vendorMapping.json
 *   4) /usr/share/linuxfancontrol/vendorMapping.json
 *
 * JSON schema (array of rules):
 * [
 *   { "regex": "(?i)^amdgpu",    "vendor": "AMD",    "priority": 100 },
 *   { "regex": "(?i)^nvidia",    "vendor": "NVIDIA", "priority": 100 },
 *   { "regex": "(?i)^(i915|intel)", "vendor": "Intel", "priority": 100 },
 *   { "regex": "(?i)acpitz",     "vendor": "ACPI",   "priority": 10 }
 * ]
 *
 * Notes:
 * - regex: ECMAScript (std::regex) by default; embed (?i) for case-insensitive.
 * - priority: higher wins. If equal, first rule wins.
 *
 * If no JSON provides a match, built-in defaults are used as final fallback.
 */
class VendorMapping {
public:
    enum class WatchMode {
        MTime,    // check file mtime every throttleMs (default)
        Inotify   // reload instantly via inotify if available
    };

    static VendorMapping& instance();

    // Configure hot-reload behavior. Can be called anytime (thread-safe).
    void setWatchMode(WatchMode mode, int throttleMs = 3000);

    // Returns a nice vendor string for a given hwmon "name" content.
    // Performs lazy loading and hot-reload when the file timestamp changes.
    // Back-compat + convenience aliases (forward to vendorForChipName)
    std::string vendorFor(const std::string& chipName)            { return vendorForChipName(chipName); }
    std::string vendorFor(std::string_view chipNameView)          { return vendorForChipName(std::string(chipNameView)); }


    // Optional: override search paths programmatically (mainly for tests or special setups).
    void setSearchPaths(std::vector<std::filesystem::path> paths);

    // Optional: override search path programmatically (absolute path or empty to clear).
    void setOverridePath(const std::string& path);

    // Lookup vendor for a given chip name. Performs lazy load & hot reload.
    std::string vendorForChipName(const std::string& chipName);

private:
    VendorMapping() = default;
    ~VendorMapping();
    VendorMapping(const VendorMapping&) = delete;
    VendorMapping& operator=(const VendorMapping&) = delete;

    // Loading
    void ensureLoaded();
    void tryReloadIfChanged();
    bool loadFromFile(const std::filesystem::path& p, std::string& err);
    std::vector<std::filesystem::path> defaultSearchPaths() const;
    std::optional<std::filesystem::file_time_type> safe_mtime(const std::filesystem::path& p) const;

    // Fallback if no mapping
    static std::string fallbackVendor(const std::string& chipName);

    // inotify support (Linux only)
    void initInotifyUnlocked();   // requires unique_lock held & activePath_ set
    void shutdownInotifyUnlocked();
    bool inotifyDrainEventsUnlocked(); // non-blocking; returns true if "change" detected

private:
    // Internal rule type is intentionally hidden (std::regex lives in .cpp)
    struct Rule;

    mutable std::shared_mutex          mtx_;
    std::vector<Rule>                  rules_;
    std::filesystem::path              activePath_;
    std::optional<std::filesystem::file_time_type> activeMtime_;
    std::string                        overridePath_;

    // Watch config/state
    WatchMode                          mode_{WatchMode::MTime};
    int                                throttleMs_{3000};
    std::chrono::steady_clock::time_point lastCheck_{};

    // inotify
    int                                inotifyFd_{-1};
    int                                inotifyWd_{-1};
    bool                               inotifyArmed_{false};

};

} // namespace lfc
