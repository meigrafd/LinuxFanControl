/*
 * Linux Fan Control — Profile model
 * - Rules combine one PWM target with one or more temperature source-curves
 * - Mix function aggregates multiple source curve percents (Max/Avg)
 * - JSON (de)serialization compatible with RPC and profile files
 * (c) 2025 LinuxFanControl contributors
 */
#pragma once

#include <string>
#include <vector>

namespace lfc {

enum class MixFunction {
    Max = 0,
    Avg = 1
};

enum class SourceKind {
    Table = 0,
    Trigger = 1
};

struct CurvePoint {
    double tempC {0.0};
    int    percent {0}; // 0..100
};

struct SourceSettings {
    int    minPercent   {20};
    int    maxPercent   {100};
    bool   stopBelowMin {false};
    double hysteresisC  {0.0};
    int    spinupPercent{0};
    int    spinupMs     {0};
};

struct SourceCurve {
    std::string                 label;     // optional display name of the source/curve
    SourceKind                  kind{SourceKind::Table};
    std::vector<std::string>    tempPaths; // hwmon temp*_input absolute paths
    std::vector<CurvePoint>     points;    // piecewise linear curve
    SourceSettings              settings;
};

struct Rule {
    std::string              pwmPath;   // hwmon pwm path
    std::vector<SourceCurve> sources;   // one-or-many sources feeding this PWM
    MixFunction              mixFn {MixFunction::Max};
};

struct Profile {
    std::string       name;   // optional human-readable name
    std::vector<Rule> rules;

    bool empty() const { return rules.empty(); }

    // JSON I/O implemented in Profile.cpp
    bool loadFromFile(const std::string& path, std::string* err = nullptr);
    bool saveToFile(const std::string& path, std::string* err = nullptr) const;
};

} // namespace lfc
