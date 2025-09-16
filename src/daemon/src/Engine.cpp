/*
 * Linux Fan Control — Engine (implementation)
 * - Periodic sensor readout and telemetry
 * - Profile application and evaluation
 * - CPU load gating by deltaC and forceTickMs
 * (c) 2025 LinuxFanControl contributors
 */
#include "Engine.hpp"
#include "Hwmon.hpp"
#include "Log.hpp"

#include <chrono>
#include <sstream>
#include <string>
#include <utility>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <iomanip>
#include <nlohmann/json.hpp>

namespace lfc {

using nlohmann::json;

static inline int clampPct(int v) {
    if (v < 0) return 0;
    if (v > 100) return 100;
    return v;
}

static int tempMilliCForId(const std::string& id,
                           const std::vector<std::pair<std::string, int>>& temps_mC,
                           int fallbackMilliC) {
    if (!id.empty()) {
        for (const auto& kv : temps_mC) {
            if (kv.first.find(id) != std::string::npos) {
                return kv.second;
            }
        }
    }
    return fallbackMilliC;
}

static std::string fmt2(double v) {
    std::ostringstream oss;
    oss.setf(std::ios::fixed);
    oss << std::setprecision(2) << v;
    return oss.str();
}

Engine::Engine() = default;
Engine::~Engine() { stop(); }

void Engine::setSnapshot(const HwmonSnapshot& snap) { snap_ = snap; }
bool Engine::initShm(const std::string& path) { return shm_.openOrCreate(path, 1 << 20); }
void Engine::start() { running_ = true; lastTick_ = std::chrono::steady_clock::now(); lastWork_ = lastTick_; }
void Engine::stop() { running_ = false; shm_.close(); }
void Engine::enableControl(bool on) { controlEnabled_ = on; }

void Engine::setGating(double deltaC, int forceTickMs) {
    if (deltaC < 0.0) deltaC = 0.0;
    if (deltaC > 10.0) deltaC = 10.0;
    if (forceTickMs < 100) forceTickMs = 100;
    if (forceTickMs > 10000) forceTickMs = 10000;
    gateDeltaC_ = deltaC;
    gateForceTickMs_ = forceTickMs;
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
                    ce.pts_mC_pct.emplace_back(mC, std::clamp(pct, 0, 100));
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

        b.pwmIndex = -1;
        for (size_t i = 0; i < snap_.pwms.size(); ++i) {
            const auto& p = snap_.pwms[i].path_pwm;
            if (p == c.Identifier || (!c.Identifier.empty() && p.find(c.Identifier) != std::string::npos)) {
                b.pwmIndex = static_cast<int>(i);
                break;
            }
        }
        if (b.pwmIndex < 0) continue;

        b.mode = 0;
        b.curveName = c.SelectedFanCurve.Name;

        for (const auto& fc : profile_.FanCurves) {
            if (fc.Name == c.SelectedFanCurve.Name) {
                if (fc.CommandMode == 1) {
                    b.mode = 1;
                    b.mix.func = fc.SelectedMixFunction;  // 0=max, 1=min, 2=avg
                    b.mix.refNames.clear();
                    for (const auto& r : fc.SelectedFanCurves) {
                        b.mix.refNames.push_back(r.Name);
                    }
                } else if (fc.CommandMode == 2) {
                    b.mode = 2;
                    b.trig.tempId    = fc.TriggerTempSource.Identifier;
                    b.trig.loadPct   = fc.LoadFanSpeed;
                    b.trig.loadTempC = fc.LoadTemperature;
                    b.trig.idlePct   = fc.IdleFanSpeed;
                    b.trig.idleTempC = fc.IdleTemperature;
                }
                break;
            }
        }

        bindings_.push_back(std::move(b));
    }
}

int Engine::evalCurve(const CurveEval& c, int mC) const {
    if (c.pts_mC_pct.empty()) {
        if (mC <= 40000) return 20;
        if (mC >= 80000) return 100;
        double t = (mC - 40000) / 40000.0;
        return std::clamp(static_cast<int>(20 + t * 80), 0, 100);
    }
    if (mC <= c.pts_mC_pct.front().first) return std::clamp(c.pts_mC_pct.front().second, 0, 100);
    if (mC >= c.pts_mC_pct.back().first)  return std::clamp(c.pts_mC_pct.back().second, 0, 100);
    for (size_t i = 1; i < c.pts_mC_pct.size(); ++i) {
        if (mC < c.pts_mC_pct[i].first) {
            int x0 = c.pts_mC_pct[i - 1].first, y0 = c.pts_mC_pct[i - 1].second;
            int x1 = c.pts_mC_pct[i].first,     y1 = c.pts_mC_pct[i].second;
            double t = double(mC - x0) / double(x1 - x0);
            return std::clamp(static_cast<int>(y0 + t * (y1 - y0)), 0, 100);
        }
    }
    return std::clamp(c.pts_mC_pct.back().second, 0, 100);
}

int Engine::evalCurveByName(const std::string& name, int mC) const {
    auto it = curves_.find(name);
    if (it == curves_.end()) {
        if (mC <= 40000) return 20;
        if (mC >= 80000) return 100;
        double t = (mC - 40000) / 40000.0;
        return std::clamp(static_cast<int>(20 + t * 80), 0, 100);
    }
    return evalCurve(it->second, mC);
}

int Engine::evalMixAtOwnSources(const MixEval& m,
                                const std::unordered_map<std::string, CurveEval>& curves,
                                const std::vector<std::pair<std::string, int>>& temps_mC,
                                int fallbackMilliC) const {
    if (m.refNames.empty()) return 0;

    int minv = 101, maxv = -1, sum = 0, cnt = 0;

    for (const auto& ref : m.refNames) {
        auto it = curves.find(ref);
        if (it == curves.end()) continue;

        const CurveEval& c = it->second;
        int mC = tempMilliCForId(c.tempId, temps_mC, fallbackMilliC);
        int v  = evalCurve(c, mC);

        minv = std::min(minv, v);
        maxv = std::max(maxv, v);
        sum += v;
        ++cnt;
    }

    if (cnt == 0) return 0;
    if (m.func == 1) return std::clamp(minv, 0, 100);
    if (m.func == 2) return std::clamp(static_cast<int>(sum / cnt), 0, 100);
    return std::clamp(maxv, 0, 100);
}

int Engine::evalTrigger(const TriggerEval& t, int mC) const {
    const int idleT = t.idleTempC * 1000;
    const int loadT = t.loadTempC * 1000;
    const int idleP = std::clamp(t.idlePct, 0, 100);
    const int loadP = std::clamp(t.loadPct, 0, 100);
    if (mC <= idleT) return idleP;
    if (mC >= loadT) return loadP;
    const double r = double(mC - idleT) / double(loadT - idleT);
    return std::clamp(static_cast<int>(idleP + r * (loadP - idleP)), 0, 100);
}

bool Engine::gatingAllowsWork(const std::vector<std::pair<std::string,int>>& temps_mC,
                              std::chrono::steady_clock::time_point now) {
    if (std::chrono::duration_cast<std::chrono::milliseconds>(now - lastWork_).count() >= gateForceTickMs_) {
        return true;
    }
    if (lastTemps_mC_.empty()) return true;

    const int thr_mC = static_cast<int>(gateDeltaC_ * 1000.0 + 0.5);
    int maxAbs = 0;
    for (const auto& kv : temps_mC) {
        int prev = 0; bool havePrev = false;
        for (const auto& pv : lastTemps_mC_) {
            if (pv.first == kv.first) { prev = pv.second; havePrev = true; break; }
        }
        if (!havePrev) { maxAbs = thr_mC; break; }
        int d = std::abs(kv.second - prev);
        if (d > maxAbs) maxAbs = d;
        if (maxAbs >= thr_mC) break;
    }
    return maxAbs >= thr_mC;
}

void Engine::tick() {
    if (!running_) return;

    auto now = std::chrono::steady_clock::now();

    // Read sensors
    std::vector<std::pair<std::string, int>> temps_mC;
    temps_mC.reserve(snap_.temps.size());
    std::vector<std::pair<std::string, double>> tempsC_2dp;
    tempsC_2dp.reserve(snap_.temps.size());

    for (const auto& t : snap_.temps) {
        auto mC = Hwmon::readMilliC(t);
        if (mC) {
            const std::string name = t.label.empty() ? t.path_input : t.label;
            temps_mC.emplace_back(name, *mC);
            double c = static_cast<double>(*mC) / 1000.0;
            tempsC_2dp.emplace_back(name, c);
        }
    }

    // Optional skip if gating says "no change"
    if (!gatingAllowsWork(temps_mC, now)) {
        json j;
        j["temps"] = json::array();
        for (const auto& kv : tempsC_2dp) {
            j["temps"].push_back({{"name", kv.first}, {"c", fmt2(kv.second)}});
        }
        j["fans"] = json::array();
        j["pwm"] = json::array();
        lastTelemetry_ = j.dump();
        (void)shm_.appendJsonLine(lastTelemetry_);
        return;
    }

    lastWork_ = now;
    lastTemps_mC_ = temps_mC;

    std::vector<std::pair<std::string, int>> fans;
    fans.reserve(snap_.fans.size());
    for (const auto& f : snap_.fans) {
        auto v = Hwmon::readRpm(f);
        if (v) fans.emplace_back(f.path_input, *v);
    }

    int fallbackMaxMilliC = 0;
    for (const auto& kv : temps_mC) {
        if (kv.second > fallbackMaxMilliC) fallbackMaxMilliC = kv.second;
    }

    std::vector<int> appliedPct(snap_.pwms.size(), -1);
    if (controlEnabled_) {
        for (const auto& b : bindings_) {
            if (b.pwmIndex < 0 || b.pwmIndex >= (int)snap_.pwms.size()) continue;

            int cmdPct = 0;

            if (b.mode == 0) {
                auto it = curves_.find(b.curveName);
                if (it != curves_.end()) {
                    const CurveEval& c = it->second;
                    int mC = tempMilliCForId(c.tempId, temps_mC, fallbackMaxMilliC);
                    cmdPct = evalCurve(c, mC);
                } else {
                    cmdPct = evalCurveByName(b.curveName, fallbackMaxMilliC);
                }
            } else if (b.mode == 1) {
                cmdPct = evalMixAtOwnSources(b.mix, curves_, temps_mC, fallbackMaxMilliC);
            } else if (b.mode == 2) {
                int mC = tempMilliCForId(b.trig.tempId, temps_mC, fallbackMaxMilliC);
                cmdPct = evalTrigger(b.trig, mC);
            }

            cmdPct = std::max(cmdPct, b.minPercent);
            Hwmon::setManual(snap_.pwms[b.pwmIndex]);
            Hwmon::setPercent(snap_.pwms[b.pwmIndex], cmdPct);
            appliedPct[b.pwmIndex] = cmdPct;
        }
    }

    json j;
    j["temps"] = json::array();
    for (const auto& kv : tempsC_2dp) {
        j["temps"].push_back({{"name", kv.first}, {"c", fmt2(kv.second)}});
    }
    j["fans"] = json::array();
    for (const auto& kv : fans) {
        j["fans"].push_back({{"path", kv.first}, {"rpm", kv.second}});
    }
    j["pwm"] = json::array();
    for (int v : appliedPct) j["pwm"].push_back(v);

    lastTelemetry_ = j.dump();
    (void)shm_.appendJsonLine(lastTelemetry_);
}

bool Engine::getTelemetry(std::string& out) const {
    if (lastTelemetry_.empty()) return false;
    out = lastTelemetry_;
    return true;
}

} // namespace lfc
