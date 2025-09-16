/*
 * Linux Fan Control — Engine (header)
 * - Periodic control loop and telemetry
 * - Profile-driven curve/mix/trigger evaluation
 * (c) 2025 LinuxFanControl contributors
 */
#pragma once

#include "Hwmon.hpp"
#include "ShmTelemetry.hpp"
#include "Profile.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <chrono>

namespace lfc {

class Engine {
public:
    Engine();
    ~Engine();

    void setSnapshot(const HwmonSnapshot& snap);
    bool initShm(const std::string& path);

    void start();
    void stop();

    void enableControl(bool on);
    bool controlEnabled() const { return controlEnabled_; }

    // Telemetry (last JSON line)
    bool getTelemetry(std::string& out) const;

    void applyProfile(const Profile& p);

    // Gating parameters (runtime only; config lives in Config.hpp)
    void setGating(double deltaC, int forceTickMs);

    // Called by daemon each loop tick; may skip heavy work if gating says so
    void tick();

private:
    struct CurveEval {
        std::string name;
        std::string tempId;
        int maxTempC{100};
        int minTempC{0};
        int maxCmd{100};
        std::vector<std::pair<int,int>> pts_mC_pct;  // (milliC, percent)
    };

    struct MixEval {
        int func{0};                       // 0=max, 1=min, 2=avg
        std::vector<std::string> refNames; // referenced curve names
    };

    struct TriggerEval {
        std::string tempId;
        int idleTempC{40};
        int loadTempC{80};
        int idlePct{20};
        int loadPct{100};
    };

    struct Binding {
        int pwmIndex{-1};
        int minPercent{0};
        int mode{0};                       // 0=curve, 1=mix, 2=trigger
        std::string curveName;
        MixEval mix;
        TriggerEval trig;
    };

private:
    void buildBindingsFromProfile();

    int evalCurve(const CurveEval& c, int mC) const;
    int evalCurveByName(const std::string& name, int mC) const;
    int evalMixAtOwnSources(const MixEval& m,
                            const std::unordered_map<std::string, CurveEval>& curves,
                            const std::vector<std::pair<std::string,int>>& temps_mC,
                            int fallbackMilliC) const;
    int evalTrigger(const TriggerEval& t, int mC) const;

    bool gatingAllowsWork(const std::vector<std::pair<std::string,int>>& temps_mC,
                          std::chrono::steady_clock::time_point now);

private:
    HwmonSnapshot snap_{};
    Profile profile_{};

    bool running_{false};
    bool controlEnabled_{true};

    // Runtime gating (NOT config; config lives in Config.hpp / Daemon)
    double gateDeltaC_{0.5};                        // °C
    int    gateForceTickMs_{2000};                  // ms
    std::chrono::steady_clock::time_point lastWork_{};
    std::vector<std::pair<std::string,int>> lastTemps_mC_;

    std::chrono::steady_clock::time_point lastTick_{};

    std::unordered_map<std::string, CurveEval> curves_;
    std::vector<Binding> bindings_;

    ShmTelemetry shm_;
    std::string lastTelemetry_;
};

} // namespace lfc
