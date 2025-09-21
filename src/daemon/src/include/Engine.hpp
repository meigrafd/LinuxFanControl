/*
 * Linux Fan Control — Engine (header)
 * - Applies Profile to hwmon devices with mixing, curves, hysteresis and spin-up
 * (c) 2025 LinuxFanControl contributors
 */
#pragma once

#include "Profile.hpp"
#include "Hwmon.hpp"

#include <string>
#include <vector>
#include <chrono>

namespace lfc {

class Engine {
public:
    Engine();
    ~Engine();

    void setHwmonView(const std::vector<HwmonTemp>& temps,
                      const std::vector<HwmonFan>&  fans,
                      const std::vector<HwmonPwm>&  pwms);

    void applyProfile(const Profile& p);

    bool tick(double deltaC);

private:
    struct RuleState {
        bool   hasLastTemp{false};
        double lastTempC{0.0};
        double prevTempC{0.0};
        int    lastPercent{-1};
        std::chrono::steady_clock::time_point spinUntil{};
    };

    const HwmonTemp* findTempSensor(const std::string& path) const;
    const HwmonPwm*  findPwm(const std::string& path) const;

    int  curvePercent(const FanCurveMeta& curve, double tempC) const;
    int  applyHysteresis(RuleState& st, int target);
    int  clamp01(int v) const;

private:
    std::vector<HwmonTemp> temps_;
    std::vector<HwmonFan>  fans_;
    std::vector<HwmonPwm>  pwms_;

    Profile profile_;

    std::vector<RuleState> ruleState_;
};

} // namespace lfc
