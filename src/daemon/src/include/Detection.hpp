/*
 * Linux Fan Control — Detection (header)
 * - Auto-detection utilities, tuning parameters, progress reporting
 * (c) 2025 LinuxFanControl contributors
 */
#pragma once

#include <atomic>
#include <functional>
#include <string>
#include <vector>

#include "Hwmon.hpp"
#include "Profile.hpp"
#include "Curve.hpp"

namespace lfc {

struct DetectionConfig {
    int settleMs{250};
    int spinupCheckMs{5000};
    int spinupPollMs{100};
    int measureTotalMs{10000};
    int rpmDeltaThresh{30};
    int rampStartPercent{30};
    int rampEndPercent{100};
    int modeDwellMs{600};
    int maxPwmToggleTries{3};
    int minValidRpm{200};
    int minValidPoints{2};
};

enum class DetectStage {
    Init,
    Probing,
    SpinupCheck,
    MeasureCurve,
    Aggregate,
    BuildProfile,
    Done,
    Error
};

using DetectProgressFn = std::function<void(int, DetectStage, const std::string&)>;

struct DetectResult {
    Profile profile;
    bool ok{false};
    std::string error;
    int mappedPwms{0};
    int mappedTemps{0};
};

class Detection {
public:
    explicit Detection(const DetectionConfig& cfg);
    ~Detection();

    void setProgressCallback(DetectProgressFn cb) { progress_ = std::move(cb); }
    const DetectionConfig& config() const noexcept { return cfg_; }
    void setConfig(const DetectionConfig& c) noexcept { cfg_ = c; }

    bool runAutoDetect(const HwmonSnapshot& hw, DetectResult& result);

    void requestStop() noexcept { stop_.store(true, std::memory_order_relaxed); }

private:
    void report_(int pct, DetectStage st, const std::string& msg);

    bool ensureManualMode_(const HwmonPwm& p);
    bool restoreMode_(const HwmonPwm& p, int prevMode, int prevRaw);

    bool spinupCheck_(const HwmonPwm& p, bool& ok);
    bool measureCurve_(const HwmonPwm& p,
                       const std::vector<std::string>& tempPaths,
                       std::vector<CurvePoint>& out);

    static int  readRpmSafe_(const HwmonFan& f, int fallback = -1);
    static bool sleepMsCancelable_(int ms, const std::atomic<bool>& stopFlag);

private:
    DetectionConfig  cfg_;
    DetectProgressFn progress_;
    std::atomic<bool> stop_{false};
};

} // namespace lfc
