/*
 * Linux Fan Control — Engine (applies Profile to hardware)
 * - Drives PWMs based on active Profile and sensor readings
 * - Builds telemetry JSON (with 2-decimal temperatures) and can write via ShmTelemetry
 * (c) 2025 LinuxFanControl contributors
 */
#pragma once
#include "Profile.hpp"
#include "Hwmon.hpp"        // HwmonTemp/HwmonFan/HwmonPwm + namespace helpers
#include <vector>
#include <chrono>
#include <string>

namespace lfc {

class ShmTelemetry;

class Engine {
public:
    Engine() = default;

    // Bind current hwmon vectors (must remain valid or be rebound after rescan).
    void setHwmonView(const std::vector<HwmonTemp>& temps,
                      const std::vector<HwmonFan>& fans,
                      const std::vector<HwmonPwm>& pwms);

    void applyProfile(const Profile& p);
    Profile currentProfile() const { return profile_; }

    void enable(bool en) { enabled_ = en; }
    bool enabled() const { return enabled_; }

    // Returns true if at least one PWM was updated.
    bool tick(double deltaC);

    // Optional: attach SHM writer for telemetry updates after ticks.
    void attachTelemetry(ShmTelemetry* w) { telemetry_ = w; }

    // Build telemetry JSON snapshot (temps/fans/pwms + basic engine state).
    // Temperatures are strings with 2 decimals.
    std::string telemetryJson() const;

private:
    // Views to discovered hardware (not owned)
    const std::vector<HwmonTemp>* temps_{nullptr};
    const std::vector<HwmonFan>*  fans_{nullptr};
    const std::vector<HwmonPwm>*  pwms_{nullptr};

    Profile profile_;
    bool enabled_{false};

    // Per-rule state for hysteresis/spin-up
    std::vector<double> lastTemps_;     // last max temp per rule
    std::vector<int> lastDuty_;         // last applied duty per rule
    std::vector<std::chrono::steady_clock::time_point> spinUntil_;

    // Optional SHM writer (owned by caller)
    ShmTelemetry* telemetry_{nullptr};

    // Curve evaluation
    static int lerpPoints(const std::vector<CurvePoint>& pts, double tC);
    static int evalCurvePercent(const SourceCurve& sc, double tC);
    static int clampDuty(const SourceCurve& sc, int duty);
    static int aggregatePercents(const std::vector<int>& percs, MixFunction fn);

    // Lookup helpers (path -> Hwmon object)
    const HwmonTemp* findTempByPath(const std::string& path) const;
    const HwmonPwm*  findPwmByPath(const std::string& path) const;

    // Telemetry helpers
    static std::string fmt2(double v);
};

} // namespace lfc
