/*
 * Linux Fan Control — FanControl.Release importer (public API)
 * (c) 2025 LinuxFanControl contributors
 */
#pragma once
#include <functional>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include "Profile.hpp"
#include "Hwmon.hpp"

namespace lfc {

class FanControlImport {
public:
    using ProgressFn = std::function<void(int,const std::string&)>;

    // Returns true on success. On failure, 'err' is filled.
    // If 'detailsOut' is non-null, it will be filled with diagnostic/mapping details.
    static bool LoadAndMap(const std::string& path,
                           const std::vector<HwmonTemp>& temps,
                           const std::vector<HwmonPwm>& pwms,
                           Profile& out,
                           std::string& err,
                           ProgressFn onProgress,
                           nlohmann::json* detailsOut = nullptr);
};

} // namespace lfc
