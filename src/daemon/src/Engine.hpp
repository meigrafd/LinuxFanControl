/*
 * Linux Fan Control — Engine (header)
 * - Periodic sensor readout and telemetry
 * - Profile application (FanControl.Releases compatible)
 * - Curve / Mix / Trigger evaluation per control
 * - SHM telemetry via ShmTelemetry
 * (c) 2025 LinuxFanControl contributors
 */
#pragma once
#include <string>
#include <chrono>
#include <vector>
#include <unordered_map>
#include "Hwmon.hpp"
#include "Config.hpp"
#include "ShmTelemetry.hpp"

namespace lfc {

class Engine {
public:
    Engine();
    ~Engine();

    void setSnapshot(const HwmonSnapshot& snap);
    bool initShm(const std::string& path);
    void start();
    void stop();
    void tick();

    void enableControl(bool on);
    bool controlEnabled() const { return controlEnabled_; }

    bool getTelemetry(std::string& out) const;

    void applyProfile(const Profile& p);

private:
    struct CurveEval {
        std::string name;
        std::vector<std::pair<int,int>> pts_mC_pct;   // (mC, percent), sorted by mC
        std::string tempId;
        int maxTempC{120};
        int minTempC{20};
        int maxCmd{100};
    };
    struct MixEval {
        int func{0};                                  // 0=max, 1=min, 2=avg
        std::vector<std::string> refNames;            // names of referenced curves
    };
    struct TriggerEval {
        std::string tempId;
        int loadPct{80};
        int loadTempC{65};
        int idlePct{30};
        int idleTempC{50};
    };
    struct Binding {
        int pwmIndex{-1};
        int mode{0};                                  // 0=curve, 1=mix, 2=trigger
        std::string curveName;                        // for curve or mix/trigger name
        MixEval mix;
        TriggerEval trig;
        int minPercent{0};
    };

private:
    int evalCurve(const CurveEval& c, int mC) const;
    int evalCurveByName(const std::string& name, int mC) const;
    int evalMix(const MixEval& m, int mC) const;
    int evalTrigger(const TriggerEval& t, int mC) const;

    int tempMilliCForId(const std::string& id,
                        const std::vector<std::pair<std::string,int>>& temps_mC,
                        int fallbackMax) const;

    void buildBindingsFromProfile();

private:
    HwmonSnapshot snap_;

    bool running_{false};
    std::chrono::steady_clock::time_point lastTick_{};

    bool controlEnabled_{false};

    Profile profile_;
    std::unordered_map<std::string, CurveEval> curves_;
    std::vector<Binding> bindings_;

    // SHM telemetry
    ShmTelemetry shm_;
    mutable std::string lastTelemetry_; // for RPC readback
};

} // namespace lfc
