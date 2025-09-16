/*
 * Linux Fan Control — FanControl.Releases import (header)
 * - Maps external config to internal Profile
 * (c) 2025 LinuxFanControl contributors
 */
#pragma once
#include <string>
#include "Config.hpp"
#include "Hwmon.hpp"

namespace lfc {

struct FanControlImport {
    static bool LoadAndMap(const std::string& path, const HwmonSnapshot& snap, Profile& out, std::string& err);
};

} // namespace lfc
