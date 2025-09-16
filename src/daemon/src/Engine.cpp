/*
 * Linux Fan Control — Engine (implementation)
 * - Applies Profile rules to hwmon outputs
 * - Builds telemetry JSON
 * (c) 2025 LinuxFanControl contributors
 */
#include "Engine.hpp"
#include "ShmTelemetry.hpp"
#include "Hwmon.hpp"
#include <nlohmann/json.hpp>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <chrono>

namespace lfc {

void Engine::setHwmonView(const std::vector<HwmonTemp>& temps,
                          const std::vector<HwmonFan>& fans,
                          const std::vector<HwmonPwm>& pwms) {
    temps_ = &temps;
    fans_  = &fans;
    pwms_  = &pwms;
}

void Engine::applyProfile(const Profile& p) {
    profile_ = p;
    lastTemps_.assign(profile_.rules.size(), 0.0);
    lastDuty_.assign(profile_.rules.size(), -1);
    spinUntil_.assign(profile_.rules.size(), std::chrono::steady_clock::time_point{});
}

int Engine::lerpPoints(const std::vector<CurvePoint>& pts, double tC) {
    if (pts.empty()) return 0;
    if (tC <= pts.front().tempC) return pts.front().percent;
    if (tC >= pts.back().tempC)  return pts.back().percent;
    for (size_t i = 1; i < pts.size(); ++i) {
        const auto& a = pts[i - 1];
        const auto& b = pts[i];
        if (tC <= b.tempC) {
            double u = (tC - a.tempC) / (b.tempC - a.tempC);
            double v = a.percent + u * (b.percent - a.percent);
            int iv = static_cast<int>(v + 0.5);
            return std::max(0, std::min(100, iv));
        }
    }
    return pts.back().percent;
}

int Engine::evalCurvePercent(const SourceCurve& sc, double tC) {
    return lerpPoints(sc.points, tC);
}

int Engine::clampDuty(const SourceCurve& sc, int duty) {
    duty = std::max(0, std::min(100, duty));
    duty = std::max(sc.settings.minPercent, std::min(sc.settings.maxPercent, duty));
    if (sc.settings.stopBelowMin && duty < sc.settings.minPercent) return 0;
    return duty;
}

int Engine::aggregatePercents(const std::vector<int>& percs, MixFunction fn) {
    if (percs.empty()) return 0;
    if (fn == MixFunction::Avg) {
        long sum = 0;
        for (int p : percs) sum += p;
        int avg = static_cast<int>((sum + static_cast<long>(percs.size() / 2)) / static_cast<long>(percs.size()));
        return std::max(0, std::min(100, avg));
    }
    int m = 0;
    for (int p : percs) m = std::max(m, p);
    return m;
}

const HwmonTemp* Engine::findTempByPath(const std::string& path) const {
    if (!temps_) return nullptr;
    for (const auto& t : *temps_) {
        if (t.path_input == path) return &t;
    }
    return nullptr;
}

const HwmonPwm* Engine::findPwmByPath(const std::string& path) const {
    if (!pwms_) return nullptr;
    for (const auto& p : *pwms_) {
        if (p.path_pwm == path) return &p;
    }
    return nullptr;
}

bool Engine::tick(double /*deltaC*/) {
    if (!enabled_) return false;
    if (profile_.empty()) return false;
    if (!temps_ || !pwms_) return false;

    bool anyChanged = false;
    auto now = std::chrono::steady_clock::now();

    for (size_t i = 0; i < profile_.rules.size(); ++i) {
        const auto& rule = profile_.rules[i];
        const HwmonPwm* pwm = findPwmByPath(rule.pwmPath);
        if (!pwm) continue;

        // Collect source percents (each source may reference multiple temps -> take max per source)
        std::vector<int> percs;
        double ruleMaxTemp = 0.0;

        for (const auto& sc : rule.sources) {
            int srcPerc = 0;
            double srcMaxTemp = 0.0;

            for (const auto& tpath : sc.tempPaths) {
                const HwmonTemp* tp = findTempByPath(tpath);
                if (!tp) continue;
                auto tc = Hwmon::readTempC(*tp);                  // °C (double)
                if (!tc.has_value()) continue;
                srcMaxTemp = std::max(srcMaxTemp, *tc);
                int p = evalCurvePercent(sc, *tc);
                p = clampDuty(sc, p);
                srcPerc = std::max(srcPerc, p);
            }

            ruleMaxTemp = std::max(ruleMaxTemp, srcMaxTemp);
            percs.push_back(srcPerc);
        }

        // Mix all sources
        int target = aggregatePercents(percs, rule.mixFn);

        // Hysteresis relative to last max temp per rule (kept simple here)
        double lastT = (i < lastTemps_.size()) ? lastTemps_[i] : 0.0;
        if (ruleMaxTemp + 1e-9 < lastT) {
            // cooling down; allow drop
        }
        if (i < lastTemps_.size()) lastTemps_[i] = ruleMaxTemp;

        // Spin-up window: if target just rose above min, optionally enforce spinupPercent for spinupMs
        if (i >= spinUntil_.size()) spinUntil_.resize(profile_.rules.size());
        if (i >= lastDuty_.size())  lastDuty_.resize(profile_.rules.size(), -1);

        if (lastDuty_[i] < target) {
            const auto& sc0 = rule.sources.empty() ? SourceCurve{} : rule.sources.front();
            if (sc0.settings.spinupPercent > 0 && sc0.settings.spinupMs > 0) {
                if (now >= spinUntil_[i]) {
                    target = std::max(target, sc0.settings.spinupPercent);
                    spinUntil_[i] = now + std::chrono::milliseconds(sc0.settings.spinupMs);
                }
            }
        }

        // Apply to hwmon (Hwmon::setPercent returns void in this repo)
        if (i < lastDuty_.size()) {
            if (lastDuty_[i] != target) {
                Hwmon::setPercent(*pwm, target);
                lastDuty_[i] = target;
                anyChanged = true;
            }
        }
    }

    if (anyChanged && telemetry_) {
        (void)telemetry_->update(telemetryJson());
    }
    return anyChanged;
}

std::string Engine::fmt2(double v) {
    std::ostringstream oss;
    oss.setf(std::ios::fixed);
    oss << std::setprecision(2) << v;
    return oss.str();
}

std::string Engine::telemetryJson() const {
    nlohmann::json j;

    // Temps
    if (temps_) {
        auto& arr = j["temps"] = nlohmann::json::array();
        for (const auto& t : *temps_) {
            nlohmann::json o;
            o["path"]  = t.path_input;
            o["label"] = t.label.empty() ? t.path_input : t.label;
            auto c = Hwmon::readTempC(t);
            if (c.has_value()) o["c"] = fmt2(*c);
            else               o["c"] = nullptr;
            arr.push_back(std::move(o));
        }
    }

    // Fans
    if (fans_) {
        auto& arr = j["fans"] = nlohmann::json::array();
        for (const auto& f : *fans_) {
            nlohmann::json o;
            o["path"] = f.path_input;
            o["rpm"]  = Hwmon::readRpm(f).value_or(0);
            arr.push_back(std::move(o));
        }
    }

    // PWMs
    if (pwms_) {
        auto& arr = j["pwms"] = nlohmann::json::array();
        for (const auto& p : *pwms_) {
            nlohmann::json o;
            o["path"]    = p.path_pwm;
            o["percent"] = Hwmon::readPercent(p).value_or(-1);
            arr.push_back(std::move(o));
        }
    }

    j["engine"]["enabled"] = enabled();
    return j.dump();
}

} // namespace lfc
