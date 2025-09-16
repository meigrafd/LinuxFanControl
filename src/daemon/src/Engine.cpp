/*
 * Linux Fan Control — Engine (implementation)
 * - Periodic sensor readout and telemetry
 * - Profile application (FanControl.Releases compatible)
 * - Curve / Mix / Trigger evaluation per control
 * - SHM telemetry via ShmTelemetry
 * (c) 2025 LinuxFanControl contributors
 */
#include "Engine.hpp"
#include "Hwmon.hpp"
#include "Log.hpp"

#include <chrono>
#include <cstring>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>
#include <algorithm>

namespace lfc {

static inline int clampPct(int v) {
    if (v < 0) return 0;
    if (v > 100) return 100;
    return v;
}

Engine::Engine() = default;
Engine::~Engine() { stop(); }

void Engine::setSnapshot(const HwmonSnapshot& snap) {
    snap_ = snap;
}

bool Engine::initShm(const std::string& path) {
    return shm_.openOrCreate(path, 1 << 20);
}

void Engine::start() {
    running_ = true;
    lastTick_ = std::chrono::steady_clock::now();
}

void Engine::stop() {
    running_ = false;
    shm_.close();
}

void Engine::enableControl(bool on) {
    controlEnabled_ = on;
}

static bool parsePoint(const std::string& s, int& out_mC, int& out_pct) {
    auto c = s.find(',');
    if (c == std::string::npos) return false;
    try {
        int Xc = std::stoi(s.substr(0, c));
        int Yp = std::stoi(s.substr(c + 1));
        out_mC = Xc * 1000;
        out_pct = Yp;
        return true;
    } catch (...) {
        return false;
    }
}

void Engine::applyProfile(const Profile& p) {
    profile_ = p;
    curves_.clear();

    for (const auto& fc : profile_.FanCurves) {
        if (fc.CommandMode == 0) {
            CurveEval ce;
            ce.name = fc.Name;
            ce.tempId = fc.SelectedTempSource.Identifier;
            ce.maxTempC = fc.MaximumTemperature;
            ce.minTempC = fc.MinimumTemperature;
            ce.maxCmd = fc.MaximumCommand;
            ce.pts_mC_pct.clear();
            for (const auto& s : fc.Points) {
                int mC = 0, pct = 0;
                if (parsePoint(s, mC, pct)) {
                    ce.pts_mC_pct.emplace_back(mC, clampPct(pct));
                }
            }
            std::sort(ce.pts_mC_pct.begin(), ce.pts_mC_pct.end(),
                      [](auto& a, auto& b){ return a.first < b.first; });
            curves_[ce.name] = std::move(ce);
        }
    }

    buildBindingsFromProfile();
}

void Engine::buildBindingsFromProfile() {
    bindings_.clear();
    for (const auto& c : profile_.Controls) {
        if (!c.Enable) continue;
        Binding b;
        b.minPercent = std::max(0, c.MinimumPercent);

        // map control.Identifier to pwm index (substring match)
        b.pwmIndex = -1;
        for (size_t i = 0; i < snap_.pwms.size(); ++i) {
            const auto& p = snap_.pwms[i].path_pwm;
            if (p == c.Identifier || (!c.Identifier.empty() && p.find(c.Identifier) != std::string::npos)) {
                b.pwmIndex = static_cast<int>(i);
                break;
            }
        }
        if (b.pwmIndex < 0) continue;

        // resolve mode based on selected fan curve
        int cmdMode = 0;
        if (curves_.find(c.SelectedFanCurve.Name) != curves_.end()) {
            cmdMode = 0;
        } else {
            for (const auto& fc : profile_.FanCurves) {
                if (fc.Name == c.SelectedFanCurve.Name) {
                    cmdMode = fc.CommandMode;
                    if (cmdMode == 1) {
                        b.mix.func = fc.SelectedMixFunction;
                        b.mix.refNames.clear();
                        for (const auto& r : fc.SelectedFanCurves) b.mix.refNames.push_back(r.Name);
                    } else if (cmdMode == 2) {
                        b.trig.tempId = fc.TriggerTempSource.Identifier;
                        b.trig.loadPct = fc.LoadFanSpeed;
                        b.trig.loadTempC = fc.LoadTemperature;
                        b.trig.idlePct = fc.IdleFanSpeed;
                        b.trig.idleTempC = fc.IdleTemperature;
                    }
                    break;
                }
            }
        }

        b.mode = cmdMode;
        b.curveName = c.SelectedFanCurve.Name;
        bindings_.push_back(std::move(b));
    }
}

int Engine::tempMilliCForId(const std::string& id,
                            const std::vector<std::pair<std::string,int>>& temps_mC,
                            int fallbackMax) const {
    if (id.empty()) return fallbackMax;
    for (const auto& kv : temps_mC) {
        if (kv.first.find(id) != std::string::npos) return kv.second;
    }
    return fallbackMax;
}

int Engine::evalCurve(const CurveEval& c, int mC) const {
    if (c.pts_mC_pct.empty()) {
        if (mC <= 40000) return 20;
        if (mC >= 80000) return 100;
        double t = (mC - 40000) / 40000.0;
        return clampPct(static_cast<int>(20 + t * 80));
    }
    if (mC <= c.pts_mC_pct.front().first) return clampPct(c.pts_mC_pct.front().second);
    if (mC >= c.pts_mC_pct.back().first)  return clampPct(c.pts_mC_pct.back().second);
    for (size_t i = 1; i < c.pts_mC_pct.size(); ++i) {
        if (mC < c.pts_mC_pct[i].first) {
            int x0 = c.pts_mC_pct[i - 1].first, y0 = c.pts_mC_pct[i - 1].second;
            int x1 = c.pts_mC_pct[i].first,     y1 = c.pts_mC_pct[i].second;
            double t = double(mC - x0) / double(x1 - x0);
            return clampPct(static_cast<int>(y0 + t * (y1 - y0)));
        }
    }
    return clampPct(c.pts_mC_pct.back().second);
}

int Engine::evalCurveByName(const std::string& name, int mC) const {
    auto it = curves_.find(name);
    if (it == curves_.end()) {
        if (mC <= 40000) return 20;
        if (mC >= 80000) return 100;
        double t = (mC - 40000) / 40000.0;
        return clampPct(static_cast<int>(20 + t * 80));
    }
    return evalCurve(it->second, mC);
}

int Engine::evalMix(const MixEval& m, int mC) const {
    if (m.refNames.empty()) return 0;
    int minv = 101, maxv = -1, sum = 0;
    for (const auto& n : m.refNames) {
        int v = evalCurveByName(n, mC);
        if (v < minv) minv = v;
        if (v > maxv) maxv = v;
        sum += v;
    }
    if (m.func == 1) return clampPct(minv);
    if (m.func == 2) return clampPct(static_cast<int>(sum / (int)m.refNames.size()));
    return clampPct(maxv);
}

int Engine::evalTrigger(const TriggerEval& t, int mC) const {
    int idleT  = t.idleTempC * 1000;
    int loadT  = t.loadTempC * 1000;
    int idleP  = clampPct(t.idlePct);
    int loadP  = clampPct(t.loadPct);
    if (mC <= idleT) return idleP;
    if (mC >= loadT) return loadP;
    double r = double(mC - idleT) / double(loadT - idleT);
    return clampPct(static_cast<int>(idleP + r * (loadP - idleP)));
}

void Engine::tick() {
    if (!running_) return;

    auto now = std::chrono::steady_clock::now();
    if (now - lastTick_ < std::chrono::milliseconds(200)) return;
    lastTick_ = now;

    std::vector<std::pair<std::string, int>> temps;
    temps.reserve(snap_.temps.size());
    for (const auto& t : snap_.temps) {
        auto v = Hwmon::readMilliC(t);
        if (v) temps.emplace_back(t.label.empty() ? t.path_input : t.label, *v);
    }

    std::vector<std::pair<std::string, int>> fans;
    fans.reserve(snap_.fans.size());
    for (const auto& f : snap_.fans) {
        auto v = Hwmon::readRpm(f);
        if (v) fans.emplace_back(f.path_input, *v);
    }

    int maxMilliC = 0;
    for (const auto& kv : temps) if (kv.second > maxMilliC) maxMilliC = kv.second;

    std::vector<int> appliedPct(snap_.pwms.size(), -1);

    if (controlEnabled_) {
        for (const auto& b : bindings_) {
            if (b.pwmIndex < 0 || b.pwmIndex >= (int)snap_.pwms.size()) continue;

            int refTemp = maxMilliC;
            if (b.mode == 0) {
                auto it = curves_.find(b.curveName);
                if (it != curves_.end() && !it->second.tempId.empty()) {
                    refTemp = tempMilliCForId(it->second.tempId, temps, maxMilliC);
                }
                int pct = evalCurveByName(b.curveName, refTemp);
                pct = std::max(pct, b.minPercent);
                Hwmon::setManual(snap_.pwms[b.pwmIndex]);
                Hwmon::setPercent(snap_.pwms[b.pwmIndex], pct);
                appliedPct[b.pwmIndex] = pct;
            } else if (b.mode == 1) {
                int pct = evalMix(b.mix, refTemp);
                pct = std::max(pct, b.minPercent);
                Hwmon::setManual(snap_.pwms[b.pwmIndex]);
                Hwmon::setPercent(snap_.pwms[b.pwmIndex], pct);
                appliedPct[b.pwmIndex] = pct;
            } else if (b.mode == 2) {
                if (!b.trig.tempId.empty()) {
                    refTemp = tempMilliCForId(b.trig.tempId, temps, maxMilliC);
                }
                int pct = evalTrigger(b.trig, refTemp);
                pct = std::max(pct, b.minPercent);
                Hwmon::setManual(snap_.pwms[b.pwmIndex]);
                Hwmon::setPercent(snap_.pwms[b.pwmIndex], pct);
                appliedPct[b.pwmIndex] = pct;
            }
        }
    }

    std::ostringstream tele;
    tele << "{";
    tele << "\"control\":" << (controlEnabled_ ? "true" : "false") << ",";
    tele << "\"max_mC\":" << maxMilliC << ",";
    tele << "\"temps\":[";
    {
        bool first = true;
        for (const auto& kv : temps) {
            if (!first) tele << ",";
            first = false;
            tele << "{\"name\":\"";
            for (char c : kv.first) { if (c=='\"'||c=='\\') tele << '\\'; tele << c; }
            tele << "\",\"mC\":" << kv.second << "}";
        }
    }
    tele << "],";
    tele << "\"fans\":[";
    {
        bool first = true;
        for (const auto& kv : fans) {
            if (!first) tele << ",";
            first = false;
            tele << "{\"path\":\"";
            for (char c : kv.first) { if (c=='\"'||c=='\\') tele << '\\'; tele << c; }
            tele << "\",\"rpm\":" << kv.second << "}";
        }
    }
    tele << "],";
    tele << "\"pwms\":[";
    {
        bool first = true;
        for (size_t i = 0; i < snap_.pwms.size(); ++i) {
            if (!first) tele << ",";
            first = false;
            tele << "{\"path\":\"";
            for (char c : snap_.pwms[i].path_pwm) { if (c=='\"'||c=='\\') tele << '\\'; tele << c; }
            tele << "\",\"percent\":";
            int v = appliedPct[i];
            if (v < 0) tele << "null"; else tele << v;
            tele << "}";
        }
    }
    tele << "]";
    tele << "}";

    lastTelemetry_ = tele.str();
    shm_.appendJsonLine(lastTelemetry_);
}

bool Engine::getTelemetry(std::string& out) const {
    out = lastTelemetry_;
    return !out.empty();
}

} // namespace lfc
