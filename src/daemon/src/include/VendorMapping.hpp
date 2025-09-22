/*
 * Linux Fan Control — Vendor mapping (header)
 * (c) 2025 LinuxFanControl contributors
 */
#pragma once
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace lfc {

class VendorMapping {
public:
    enum class WatchMode { None, MTime, Inotify };

    static VendorMapping& instance();

    void setSearchPaths(const std::vector<std::filesystem::path>& paths);
    void setOverridePath(const std::filesystem::path& path);

    // Both overloads are provided (some call sites pass only the mode).
    void setWatchMode(WatchMode mode);
    void setWatchMode(WatchMode mode, int throttleMs);

    void setThrottleMs(int ms);

    void ensureLoaded();
    void tryReloadIfChanged();

    std::string vendorForChipName(const std::string& chip) const;
    std::string vendorFor(const std::string& chip) { return vendorForChipName(chip); }
    std::string vendorFor(std::string_view chip) { return vendorForChipName(std::string(chip)); }

    std::vector<std::string> chipAliasesFor(const std::string& profileToken) const;

private:
    friend class VendorMappingImpl;
    VendorMapping() = default;
};

} // namespace lfc
