/*
 * Linux Fan Control — Engine (implementation)
 * (c) 2025 LinuxFanControl contributors
 *
 * Notes:
 * - Skips disabled and manual controls in tick() to avoid bogus warnings.
 * - Logs now include the control's mapped nickName (or name) alongside the pwmPath.
 * - Replaced ambiguous "agg" and "(Δ=999.000C>=0.500C)" with clearer wording:
 *     • "avgTemp=…" for the aggregated temperature across sensors
 *     • "Δ=…°C ≥ gate=…°C" (or "Δ=n/a" on first sample)
 */

#include "include/Engine.hpp"
#include "include/Hwmon.hpp"
#include "include/Log.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

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

// Helper: choose a nice label for a control (nickName > name > fallback)
static std::string controlLabel(const ControlMeta& c, const HwmonPwm* pwm) {
    if (!c.nickName.empty()) return c.nickName;
    if (!c.name.empty())     return c.name;
    if (pwm)                 return pwm->path_pwm;
    return std::string{"(unnamed)"};
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

        // Skip disabled controls entirely
        if (!ctrl.enabled) {
            continue;
        }
        // Skip manual controls here (manual application is handled elsewhere)
        if (ctrl.manual) {
            continue;
        }

        const HwmonPwm* pwm = findPwm(ctrl.pwmPath);
        const std::string label = controlLabel(ctrl, pwm);

        if (!pwm) {
            LOG_WARN("engine: pwm not found: %s [%s]", ctrl.pwmPath.c_str(), label.c_str());
            continue;
        }

        // Resolve referenced curve
        const FanCurveMeta* curve = nullptr;
        for (const auto& c : profile_.fanCurves) {
            if (c.name == ctrl.curveRef) { curve = &c; break; }
        }
        if (!curve) {
            LOG_WARN("engine: curve not found: %s [%s -> %s]",
                     ctrl.curveRef.c_str(), label.c_str(), pwm->path_pwm.c_str());
            continue;
        }



// --- MIX CURVES: compute from referenced curves' own temps ---
if (curve->type == "mix") {
    std::vector<int> percents;
    std::vector<double> tempsForLog;
    percents.reserve(curve->curveRefs.size());
    tempsForLog.reserve(curve->curveRefs.size());
    for (const auto& refName : curve->curveRefs) {
        const FanCurveMeta* ref = nullptr;
        for (const auto& c2 : profile_.fanCurves) {
            if (c2.name == refName) { ref = &c2; break; }
        }
        if (!ref) continue;

        // collect temps for this referenced curve
        std::vector<double> tvals;
        tvals.reserve(ref->tempSensors.size());
        for (const auto& path : ref->tempSensors) {
            const HwmonTemp* t = findTempSensor(path);
            if (!t) continue;
            auto v = Hwmon::readTempC(*t);
            if (v) tvals.push_back(*v);
        }
        if (tvals.empty()) continue;

        double tref = 0.0;
        if (ref->mix == MixFunction::Min) {
            tref = *std::min_element(tvals.begin(), tvals.end());
        } else if (ref->mix == MixFunction::Max) {
            tref = *std::max_element(tvals.begin(), tvals.end());
        } else {
            double s = 0.0; for (double x : tvals) s += x;
            tref = s / static_cast<double>(tvals.size());
        }
        tempsForLog.push_back(tref);

        // percent of the referenced curve at its own temp
        percents.push_back(curvePercent(*ref, tref));
    }

    if (percents.empty()) {
        LOG_DEBUG("engine: mix '%s' has no usable referenced temps on %s [%s] -> skip tick",
                  curve->name.c_str(), pwm->path_pwm.c_str(), label.c_str());
        continue;
    }

    // combine percents according to mix function
    int mixPct = 0;
    if (curve->mix == MixFunction::Min) {
        mixPct = *std::min_element(percents.begin(), percents.end());
    } else if (curve->mix == MixFunction::Max) {
        mixPct = *std::max_element(percents.begin(), percents.end());
    } else {
        long sum = 0; for (int v : percents) sum += v;
        mixPct = static_cast<int>(std::lround(static_cast<double>(sum) / percents.size()));
    }

    // synthesize an avgTempC for logging/gating based on the same rule
    double avgTempC = 0.0;
    if (curve->mix == MixFunction::Min) {
        avgTempC = *std::min_element(tempsForLog.begin(), tempsForLog.end());
    } else if (curve->mix == MixFunction::Max) {
        avgTempC = *std::max_element(tempsForLog.begin(), tempsForLog.end());
    } else {
        double s = 0.0; for (double x : tempsForLog) s += x;
        avgTempC = s / static_cast<double>(tempsForLog.size());
    }

    // Minimum-change gate
    double deltaAbs = st.hasLastTemp ? std::fabs(avgTempC - st.lastTempC) : 0.0;
    if (st.hasLastTemp && deltaAbs < deltaC) {
        st.lastTempC = avgTempC;
        LOG_TRACE("engine: temp gate (mix): |%.3f-%.3f|=%.3f°C < gate=%.3f°C -> keep %d%% on %s [%s]",
                  avgTempC, st.prevTempC, std::fabs(avgTempC - st.prevTempC), deltaC,
                  st.lastPercent, pwm->path_pwm.c_str(), label.c_str());
        continue;
    }

    const int outPct = applyHysteresis(st, mixPct);

    // Ensure manual mode before write (tolerate devices without enable path)
    {
        auto en = Hwmon::readEnable(*pwm); // optional<int>
        if (en.has_value()) {
            if (*en != 1) {
                if (Hwmon::setEnable(*pwm, 1)) {
                    LOG_DEBUG("engine: set manual mode (enable=1) on %s [%s]", pwm->path_pwm.c_str(), label.c_str());
                } else {
                    LOG_WARN("engine: failed to set manual mode on %s [%s]", pwm->path_pwm.c_str(), label.c_str());
                }
            }
        } else {
            // No enable path exposed (common on some GPUs) -> proceed without trying and avoid scary warnings.
            LOG_TRACE("engine: no enable path for %s [%s] — assuming device handles mode automatically", pwm->path_pwm.c_str(), label.c_str());
        }
    }

    if (outPct != st.lastPercent) {
        if (Hwmon::setPercent(*pwm, outPct)) {
            if (st.hasLastTemp) {
                LOG_DEBUG("engine: set %s [%s] <- %d%% (was %d%%) @ mixTemp=%.2f°C; Δ=%.3f°C ≥ gate=%.3f°C",
                          pwm->path_pwm.c_str(), label.c_str(), outPct, st.lastPercent, avgTempC, deltaAbs, deltaC);
            } else {
                LOG_DEBUG("engine: set %s [%s] <- %d%% (was %d%%) @ mixTemp=%.2f°C; Δ=n/a (first sample), gate=%.3f°C",
                          pwm->path_pwm.c_str(), label.c_str(), outPct, st.lastPercent, avgTempC, deltaC);
            }
            st.lastPercent = outPct;
            anyChanged = true;
        } else {
            LOG_WARN("engine: setPercent failed on %s [%s] -> %d%% (mix)",
                     pwm->path_pwm.c_str(), label.c_str(), outPct);
        }
    }
    st.prevTempC = st.hasLastTemp ? st.lastTempC : avgTempC;
    st.lastTempC = avgTempC;
    st.hasLastTemp = true;
    continue; // done with this control (mix handled)
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

        // Ohne valide Sensorwerte NICHT schreiben (kein 0%-Ausfall) — nur Debug loggen
        if (tempsC.empty()) {
            LOG_DEBUG("engine: no sensor values for curve '%s' on %s [%s] -> skip tick",
                      curve->name.c_str(), pwm->path_pwm.c_str(), label.c_str());
            continue;
        }

        double avgTempC = 0.0;
        if (!tempsC.empty()) {
            if (curve->mix == MixFunction::Min) {
                avgTempC = *std::min_element(tempsC.begin(), tempsC.end());
            } else if (curve->mix == MixFunction::Max) {
                avgTempC = *std::max_element(tempsC.begin(), tempsC.end());
            } else {
                double sum = 0.0; for (double x : tempsC) sum += x;
                avgTempC = sum / static_cast<double>(tempsC.size());
            }
        }

        // Minimum-change gate to avoid flapping
        double deltaAbs = st.hasLastTemp ? std::fabs(avgTempC - st.lastTempC) : 0.0;
        if (st.hasLastTemp && deltaAbs < deltaC) {
            // Update stored temperature (small drift tracking) and continue
            st.lastTempC = avgTempC;
            LOG_TRACE("engine: temp gate: |%.3f-%.3f|=%.3f°C < gate=%.3f°C -> keep %d%% on %s [%s]",
                      avgTempC, st.prevTempC, std::fabs(avgTempC - st.prevTempC), deltaC,
                      st.lastPercent, pwm->path_pwm.c_str(), label.c_str());
            continue;
        }

        // Compute target percent from curve at current aggregate temperature
        const int targetPct = curvePercent(*curve, avgTempC);

        // Optional per-control hysteresis smoothing
        const int outPct = applyHysteresis(st, targetPct);

        // Ensure manual mode (pwm*_enable = 1) before attempting to write duty
        {
            auto en = Hwmon::readEnable(*pwm);
            if (!en || *en != 1) {
                if (Hwmon::setEnable(*pwm, 1)) {
                    LOG_DEBUG("engine: set manual mode (enable=1) on %s [%s]",
                              pwm->path_pwm.c_str(), label.c_str());
                } else {
                    LOG_WARN("engine: failed to set manual mode on %s [%s]",
                             pwm->path_pwm.c_str(), label.c_str());
                    // weiter; einige Treiber akzeptieren Duty-Writes trotzdem
                }
            }
        }

        // Apply only if resulting duty differs
        if (outPct != st.lastPercent) {
            if (Hwmon::setPercent(*pwm, outPct)) {
                if (st.hasLastTemp) {
                    LOG_DEBUG("engine: set %s [%s] <- %d%% (was %d%%) @ avgTemp=%.2f°C; Δ=%.3f°C ≥ gate=%.3f°C",
                              pwm->path_pwm.c_str(), label.c_str(),
                              outPct, st.lastPercent, avgTempC, deltaAbs, deltaC);
                } else {
                    LOG_DEBUG("engine: set %s [%s] <- %d%% (was %d%%) @ avgTemp=%.2f°C; Δ=n/a (first sample), gate=%.3f°C",
                              pwm->path_pwm.c_str(), label.c_str(),
                              outPct, st.lastPercent, avgTempC, deltaC);
                }
                st.lastPercent = outPct;
                anyChanged = true;
            } else {
                LOG_WARN("engine: setPercent failed on %s [%s] -> %d%%",
                         pwm->path_pwm.c_str(), label.c_str(), outPct);
            }
        }

        // Remember temperature for next deltaC comparison
        st.prevTempC = st.hasLastTemp ? st.lastTempC : avgTempC;
        st.lastTempC = avgTempC;
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
    // Trigger curves: use Idle/Load temperatures and speeds (new schema)
    if (curve.type == "trigger") {
        const double idleT = curve.idleTemperature;
        const double loadT = curve.loadTemperature;

        double idlePct = curve.idleFanSpeed;
        double loadPct = curve.loadFanSpeed;

        // Fallback: if speeds unset, derive from lowest/highest point if available
        if ((idlePct == 0.0 && loadPct == 0.0) && !curve.points.empty()) {
            auto itMin = std::min_element(curve.points.begin(), curve.points.end(),
                                          [](const CurvePoint& a, const CurvePoint& b){ return a.tempC < b.tempC; });
            auto itMax = std::max_element(curve.points.begin(), curve.points.end(),
                                          [](const CurvePoint& a, const CurvePoint& b){ return a.tempC < b.tempC; });
            if (itMin != curve.points.end()) idlePct = itMin->percent;
            if (itMax != curve.points.end()) loadPct = itMax->percent;
        }

        if (tempC >= loadT) {
            return clamp01(static_cast<int>(std::lround(loadPct)));
        } else if (tempC <= idleT) {
            return clamp01(static_cast<int>(std::lround(idlePct)));
        } else {
            // Between thresholds: choose the nearer target to avoid oscillation;
            // applyHysteresis() at call-site will smooth transitions further.
            const double mid = (idleT + loadT) * 0.5;
            const double pick = (tempC >= mid) ? loadPct : idlePct;
            return clamp01(static_cast<int>(std::lround(pick)));
        }
    }

    // Graph curves: piecewise linear interpolation
    if (curve.points.empty()) return 0;

    const auto& pts = curve.points;
    if (tempC <= pts.front().tempC) {
        return clamp01(static_cast<int>(std::lround(pts.front().percent)));
    }
    if (tempC >= pts.back().tempC) {
        return clamp01(static_cast<int>(std::lround(pts.back().percent)));
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
    return clamp01(static_cast<int>(std::lround(pts.back().percent)));
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
