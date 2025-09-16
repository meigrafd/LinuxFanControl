/*
 * Linux Fan Control — FanControl.Releases Import (implementation)
 * - Minimal loader placeholder: validates JSON file presence
 * (c) 2025 LinuxFanControl contributors
 */
#include "FanControlImport.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <filesystem>

using nlohmann::json;

namespace lfc {

bool FanControlImport::LoadAndMap(const std::string& path,
                                  const HwmonSnapshot& /*snap*/,
                                  Profile& /*out*/,
                                  std::string& err) {
    err.clear();
    if (!std::filesystem::exists(path)) {
        err = "file not found";
        return false;
    }
    std::ifstream f(path);
    if (!f) {
        err = "open failed";
        return false;
    }
    try {
        json j;
        f >> j;
    } catch (...) {
        err = "json parse failed";
        return false;
    }

    // Mapping in späterem Schritt ergänzen. Aktuell nur Format-/Existenzprüfung.
    return true;
}

} // namespace lfc
