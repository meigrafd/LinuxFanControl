/*
 * Linux Fan Control — FanControl.Releases Import (header)
 * - Mapping external config into internal Profile
 * (c) 2025 LinuxFanControl contributors
 */
#pragma once
#include <string>
#include "Engine.hpp"    // for Profile
#include "Hwmon.hpp"

namespace lfc {

class FanControlImport {
public:
    // Map a FanControl.Releases JSON file into an internal Profile.
    // Returns true on success. On failure 'err' contains a short reason.
    static bool LoadAndMap(const std::string& path,
                           const HwmonSnapshot& snap,
                           Profile& out,
                           std::string& err);
};

} // namespace lfc
