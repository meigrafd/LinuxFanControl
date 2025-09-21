/*
 * Linux Fan Control — FanControl.Releases importer (declaration)
 * - Reads a FanControl.Releases-style JSON and maps it to our Profile model
 * - Provides progress reporting, validation helpers, and mapping utilities
 * (c) 2025 LinuxFanControl contributors
 */
#pragma once

#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "Hwmon.hpp"
#include "Profile.hpp"

namespace lfc {

// Progress callback: onProgress(percentage 0..100, message)
using ProgressFn = std::function<void(int, const std::string&)>;

// High-level stages for UI/telemetry
enum class ImportStage {
    Init,
    Reading,
    Parsing,
    Mapping,
    Validating,
    Done,
    Error
};

// Lightweight statistics about the import
struct ImportStats {
    size_t controlsMapped{0};
    size_t tempsLinked{0};
    size_t curvesCreated{0};
};

// Extended result for richer callers (RPC, UI)
struct ImportResult {
    Profile profile;
    ImportStats stats;
    ImportStage stage{ImportStage::Done};
    std::string message;
    std::string warning;
};

// Import API
class FanControlImport {
public:
    // Minimal API: load file at `path`, map to `out`. Returns true on success.
    static bool LoadAndMap(const std::string& path,
                           const std::vector<HwmonTemp>& temps,
                           const std::vector<HwmonPwm>& pwms,
                           Profile& out,
                           std::string& err,
                           ProgressFn onProgress = nullptr);

    // Extended API: returns ImportResult with stats and stage/message.
    static std::optional<ImportResult> LoadAndMapEx(const std::string& path,
                                                    const std::vector<HwmonTemp>& temps,
                                                    const std::vector<HwmonPwm>& pwms,
                                                    std::string& err,
                                                    ProgressFn onProgress = nullptr);

    // Validate a mapped profile against current hwmon inventory.
    static bool Validate(const Profile& p,
                         const std::vector<HwmonTemp>& temps,
                         const std::vector<HwmonPwm>& pwms,
                         std::string& err);

    // Utility mappers (identifier -> sysfs path)
    static std::string MapPwmIdentifier(const std::vector<HwmonPwm>& pwms,
                                        const std::string& identifier);
    static std::string MapTempIdentifier(const std::vector<HwmonTemp>& temps,
                                         const std::string& identifier);

    // Human-readable stage name
    static const char* StageName(ImportStage s);
};

} // namespace lfc
