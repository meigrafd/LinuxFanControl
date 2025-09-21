/*
 * Linux Fan Control — Engine (implementation)
 * (c) 2025 LinuxFanControl contributors
 */
#include "include/Engine.hpp"
#include "include/Hwmon.hpp"
#include "include/Log.hpp"

#include <algorithm>
#include <cmath>
#include <chrono>

namespace lfc {

Engine::Engine() = default;
Engine::~Engine() = default;

void Engine::setHwmonView(const std::vector<HwmonTemp>& temps,
                          const std::vector<HwmonFan>&  fans,
                          const std::vector<HwmonPwm>&  pwms)
{
    temps_ = temps;
    fans_  = fans;
    pwms_  = pwms;
    LOG_DEBUG("engine: hwmon view set (temps=%zu fans=%zu pwms=%zu)", temps_.size(), fans_.size(), pwms_.size());
}

void Engine::applyProfile(const Profile& p) {
    profile_ = p;
    // Keep state vector size in sync with controls
    ruleState_.assign(profile_.controls.size(), RuleState{});
    LOG_INFO("engine: profile applied '%s' (controls=%zu curves=%zu)",
             profile_.name.c_str(), profile_.controls.size(), profile_.fanCurves.size());
}

bool Engine::tick(double deltaC) {
    bool anyChanged = false;

    // Ensure state is aligned with control count
    if (ruleState_.size() != profile_.controls.size()) {
        LOG_DEBUG("engine: resizing rule state (%zu -> %zu)",
                  ruleState_.size(), profile_.controls.size());
        ruleState_.assign(profile_.controls.size(), RuleState{});
    }

    for (size_t i = 0; i < profile_.controls.size(); ++i) {
        const auto& ctrl = profile_.controls[i];
        RuleState& st = ruleState_[i];

        const HwmonPwm* pwm = findPwm(ctrl.pwmPath);
        if (!pwm) {
            LOG_WARN("engine: pwm not found: %s", ctrl.pwmPath.c_str());
            continue;
        }

        // Resolve referenced curve
        const FanCurveMeta* curve = nullptr;
        for (const auto& c : profile_.fanCurves) {
            if (c.name == ctrl.curveRef) { curve = &c; break; }
        }
        if (!curve) {
            LOG_WARN("engine: curve not found: %s", ctrl.curveRef.c_str());
            continue;
        }

        // Aggregate temperatures from the curve's sensor list
        std::vector<double> tempsC;
        tempsC.reserve(curve->tempSensors.size());
        for (const auto& path : curve->tempSensors) {
            const HwmonTemp* t = findTempSensor(path);
            if (!t) continue;
            auto v = Hwmon::readTempC(*t);
            if (v) tempsC.push_back(*v);
        }

        double aggC = 0.0;
        if (!tempsC.empty()) {
            if (curve->mix == MixFunction::Min) {
                aggC = *std::min_element(tempsC.begin(), tempsC.end());
            } else if (curve->mix == MixFunction::Max) {
                aggC = *std::max_element(tempsC.begin(), tempsC.end());
            } else {
                double sum = 0.0; for (double x : tempsC) sum += x;
                aggC = sum / static_cast<double>(tempsC.size());
            }
        }

        // Use deltaC as a minimum-change gate to avoid flapping
        // If the aggregate temperature didn't move enough since the last tick,
        // keep the existing duty and skip recalculation/apply.
        if (st.hasLastTemp && std::fabs(aggC - st.lastTempC) < deltaC) {
            // Update the stored temperature (small drift tracking) and continue
            st.lastTempC = aggC;
            LOG_TRACE("engine: deltaC gate (|%.3f-%.3f|=%.3f < %.3f) -> keep %d%% on %s",
                      aggC, st.prevTempC, std::fabs(aggC - st.prevTempC), deltaC,
                      st.lastPercent, pwm->path_pwm.c_str());
            continue;
        }

        // Compute target percent from curve at current aggregate temperature
        const int targetPct = curvePercent(*curve, aggC);

        // Optional per-control hysteresis (if the rule state provides it)
        const int outPct = applyHysteresis(st, targetPct);

        // Apply only if resulting duty differs
        if (outPct != st.lastPercent) {
            if (Hwmon::setPercent(*pwm, outPct)) {
                LOG_DEBUG("engine: set %s <- %d%% (was %d%%) @ agg=%.2fC (Δ=%.3fC>=%.3fC)",
                          pwm->path_pwm.c_str(), outPct, st.lastPercent, aggC,
                          (st.hasLastTemp ? std::fabs(aggC - st.lastTempC) : 999.0), deltaC);
                st.lastPercent = outPct;
                anyChanged = true;
            } else {
                LOG_WARN("engine: setPercent failed on %s -> %d%%", pwm->path_pwm.c_str(), outPct);
            }
        }

        // Remember temperature for next deltaC comparison
        st.prevTempC = st.hasLastTemp ? st.lastTempC : aggC;
        st.lastTempC = aggC;
        st.hasLastTemp = true;
    }

    return anyChanged;
}

const HwmonPwm* Engine::findPwm(const std::string& path) const {
    for (const auto& p : pwms_) {
        if (p.path_pwm == path) return &p;
    }
    return nullptr;
}

const HwmonTemp* Engine::findTempSensor(const std::string& path) const {
    for (const auto& t : temps_) {
        if (t.path_input == path) return &t;
    }
    return nullptr;
}

int Engine::curvePercent(const FanCurveMeta& curve, double tempC) const {
    if (curve.points.empty()) return 0;

    const auto& pts = curve.points;
    if (tempC <= pts.front().tempC) {
        return clamp01(pts.front().percent);
    }
    if (tempC >= pts.back().tempC) {
        return clamp01(pts.back().percent);
    }

    for (size_t i = 1; i < pts.size(); ++i) {
        const CurvePoint& a = pts[i - 1];
        const CurvePoint& b = pts[i];
        if (tempC <= b.tempC) {
            const double den = std::max(1e-9, (b.tempC - a.tempC));
            const double u = (tempC - a.tempC) / den;
            const double y = static_cast<double>(a.percent) +
                             u * (static_cast<double>(b.percent) - static_cast<double>(a.percent));
            return clamp01(static_cast<int>(std::lround(y)));
        }
    }
    return clamp01(pts.back().percent);
}

int Engine::applyHysteresis(RuleState& st, int target) {
    // Keep original behavior: small smoothing by stepping toward target
    const int cur = (st.lastPercent < 0) ? 0 : st.lastPercent;
    if (target == cur) return cur;

    const int diff = target - cur;
    const int step = (diff > 0) ? std::min(diff, 5) : -std::min(-diff, 5);
    return clamp01(cur + step);
}

int Engine::clamp01(int v) const {
    return v < 0 ? 0 : (v > 100 ? 100 : v);
}

} // namespace lfc
