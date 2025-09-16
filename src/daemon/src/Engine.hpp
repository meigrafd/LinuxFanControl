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

    // Returns the last telemetry JSON line
    bool getTelemetry(std::string& out) const;

    void applyProfile(const Profile& p);
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
        std::string tempId;                // temperature source id
        int idleTempC{40};                 // °C
        int loadTempC{80};                 // °C
        int idlePct{20};                   // %
        int loadPct{100};                  // %
    };

    struct Binding {
        int pwmIndex{-1};                  // index into snapshot.pwms
        int minPercent{0};                 // minimum applied command
        int mode{0};                       // 0=curve, 1=mix, 2=trigger
        std::string curveName;             // for mode==0
        MixEval mix;                       // for mode==1
        TriggerEval trig;                  // for mode==2
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

private:
    HwmonSnapshot snap_{};
    Profile profile_{};

    bool running_{false};
    bool controlEnabled_{true};
    std::chrono::steady_clock::time_point lastTick_{};

    std::unordered_map<std::string, CurveEval> curves_;
    std::vector<Binding> bindings_;

    ShmTelemetry shm_;
    std::string lastTelemetry_;
};

} // namespace lfc
