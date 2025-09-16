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

struct CurvePoint {
    double tempC {0.0};
    int    percent {0}; // 0..100
};

struct SourceSettings {
    int  minPercent   {20};   // clamp lower bound
    int  maxPercent   {100};  // clamp upper bound
    bool stopBelowMin {false};
    double hysteresisC {0.0}; // simple temp hysteresis guard (°C)
    int  spinupPercent {0};   // optional spin-up duty
    int  spinupMs      {0};   // optional spin-up duration
};

struct SourceCurve {
    std::vector<std::string> tempPaths; // hwmon temp*_input absolute paths
    std::vector<CurvePoint>   points;    // piecewise linear curve
    SourceSettings            settings;
};

struct Rule {
    std::string              pwmPath;  // hwmon pwm path
    std::vector<SourceCurve> sources;  // one-or-many sources feeding this PWM
    MixFunction              mixFn {MixFunction::Max}; // aggregation across sources
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
