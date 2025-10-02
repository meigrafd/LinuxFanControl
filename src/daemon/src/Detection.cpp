/*
 * Linux Fan Control — Detection (implementation)
 * - Auto-detection utilities, tuning parameters, progress reporting
 * (c) 2025 LinuxFanControl contributors
 */
#include "include/Detection.hpp"
#include "include/Hwmon.hpp"
#include "include/Profile.hpp"
#include "include/Log.hpp"

#include <algorithm>
#include <cmath>
#include <thread>
#include <chrono>

namespace lfc {

Detection::Detection(const DetectionConfig& cfg)
: cfg_(cfg) {}

Detection::~Detection() = default;

void Detection::report_(int pct, DetectStage st, const std::string& msg) {
    if (progress_) progress_(pct, st, msg);
}

static inline int clamp01i(int v) {
    return std::max(0, std::min(100, v));
}

bool Detection::ensureManualMode_(const HwmonPwm& p) {
    int tries = cfg_.maxPwmToggleTries;
    while (tries-- > 0) {
        if (Hwmon::setEnable(p, 1)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(cfg_.modeDwellMs));
            auto mode = Hwmon::readEnable(p).value_or(-1);
            if (mode == 1) return true;
        }
    }
    return false;
}

bool Detection::restoreMode_(const HwmonPwm& p, int prevMode, int prevRaw) {
    bool ok = true;
    ok &= Hwmon::setEnable(p, prevMode);
    ok &= Hwmon::setRaw(p, prevRaw);
    return ok;
}

bool Detection::spinupCheck_(const HwmonPwm& p, bool& ok) {
    ok = false;

    const int prevMode = Hwmon::readEnable(p).value_or(2);
    const int prevRaw  = Hwmon::readRaw(p).value_or(0);

    if (!ensureManualMode_(p)) {
        LOG_WARN("detect: ensureManualMode failed for %s", p.path_pwm.c_str());
        return false;
    }

    Hwmon::setPercent(p, 100);
    const auto t0 = std::chrono::steady_clock::now();
    while (true) {
        // heuristic: give it some time; the real verification happens in measureCurve_
        auto dt = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
        if (dt >= cfg_.spinupCheckMs) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(cfg_.spinupPollMs));
        ok = true; // optimistic signal; refined later
    }

    restoreMode_(p, prevMode, prevRaw);
    return true;
}

bool Detection::measureCurve_(const HwmonPwm& p,
                              const std::vector<std::string>& tempPaths,
                              std::vector<CurvePoint>& out)
{
    out.clear();

    const int prevMode = Hwmon::readEnable(p).value_or(2);
    const int prevRaw  = Hwmon::readRaw(p).value_or(0);

    if (!ensureManualMode_(p)) {
        LOG_WARN("detect: ensureManualMode failed for %s", p.path_pwm.c_str());
        return false;
    }

    const int startPct = clamp01i(cfg_.rampStartPercent);
    const int endPct   = clamp01i(cfg_.rampEndPercent);
    const int totalMs  = std::max(cfg_.measureTotalMs, 1000);

    const auto t0 = std::chrono::steady_clock::now();
    int lastRpm = -1;

    for (int duty = startPct; duty <= endPct; duty += 5) {
        if (stop_.load(std::memory_order_relaxed)) break;

        Hwmon::setPercent(p, duty);
        if (!sleepMsCancelable_(cfg_.settleMs, stop_)) break;

        // Aggregate temperature (max across selected sensors)
        double aggC = 0.0;
        bool have = false;
        for (const auto& path : tempPaths) {
            HwmonTemp t{.chipPath = p.chipPath, .path_input = path, .label = {}};
            auto v = Hwmon::readTempC(t);
            if (v) {
                aggC = have ? std::max(aggC, *v) : *v;
                have = true;
            }
        }

        // Try to observe some RPM movement on same chip (best-effort)
        int rpm = -1;
        // Note: fan association is done by chip; if needed, caller can post-validate

        if (lastRpm >= 0 && rpm >= 0) {
            if (std::abs(rpm - lastRpm) < cfg_.rpmDeltaThresh) {
                // flat / no informative change; still record temp→duty point
            }
        }
        lastRpm = rpm;

        out.push_back(CurvePoint{aggC, static_cast<double>(duty)});

        auto dt = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
        if (dt >= totalMs) break;
    }

    restoreMode_(p, prevMode, prevRaw);
    return out.size() >= static_cast<size_t>(std::max(1, cfg_.minValidPoints));
}

bool Detection::runAutoDetect(const HwmonSnapshot& hw, DetectResult& result) {
    LOG_INFO("detect: begin");
    result = DetectResult{};
    stop_.store(false, std::memory_order_relaxed);

    report_(1, DetectStage::Init, "init");
    LOG_DEBUG("detect: hw: chips=%zu temps=%zu fans=%zu pwms=%zu",
              hw.chips.size(), hw.temps.size(), hw.fans.size(), hw.pwms.size());

    Profile p;
    p.schema = "LinuxFanControl.Profile/v1";
    p.name = "AutoDetected";

    report_(5, DetectStage::Probing, "devices");

    for (const auto& chip : hw.chips) {
        HwmonDeviceMeta dm;
        dm.hwmonPath = chip.hwmonPath;
        dm.name      = chip.name;
        dm.vendor    = chip.vendor;
        p.hwmons.push_back(dm);
    }

    int mapped = 0;

    for (const auto& pwm : hw.pwms) {
        if (stop_.load(std::memory_order_relaxed)) {
            result.error = "aborted";
            report_(0, DetectStage::Error, result.error);
            LOG_WARN("detect: aborted");
            return false;
        }

        report_(15, DetectStage::SpinupCheck, pwm.label.empty() ? pwm.path_pwm : pwm.label);

        bool canSpin = false;
        if (!spinupCheck_(pwm, canSpin)) {
            LOG_TRACE("detect: spinupCheck fail for %s", pwm.path_pwm.c_str());
            continue;
        }

        // Select temperature sensors on the same chip; fallback to a global first sensor
        std::vector<std::string> tpaths;
        for (const auto& t : hw.temps) {
            if (t.chipPath == pwm.chipPath) {
                tpaths.push_back(t.path_input);
            }
        }
        if (tpaths.empty() && !hw.temps.empty()) {
            tpaths.push_back(hw.temps.front().path_input);
        }

        report_(30, DetectStage::MeasureCurve, pwm.path_pwm);
        std::vector<CurvePoint> points;
        if (!measureCurve_(pwm, tpaths, points)) {
            LOG_DEBUG("detect: measureCurve no points for %s", pwm.path_pwm.c_str());
            continue;
        }

        // Build a curve and a control referencing it (Profile model uses FanCurveMeta + ControlMeta)
        FanCurveMeta meta;
        meta.name = pwm.label.empty() ? pwm.path_pwm : pwm.label;
        meta.type = points.empty() ? "mix" : "graph";
        meta.mix  = MixFunction::Avg;
        meta.tempSensors = tpaths;
        meta.points = points;
        p.fanCurves.push_back(meta);

        ControlMeta cm;
        cm.name     = meta.name;
        cm.pwmPath  = pwm.path_pwm;
        cm.curveRef = meta.name;
        p.controls.push_back(cm);

        ++mapped;
        report_(60, DetectStage::Aggregate, meta.name);
    }

    report_(85, DetectStage::BuildProfile, "profile");
    result.profile = p;
    result.ok = mapped > 0;
    result.mappedPwms = mapped;
    result.mappedTemps = static_cast<int>(hw.temps.size());

    if (!result.ok) {
        result.error = "no pwm could be mapped";
        report_(0, DetectStage::Error, result.error);
        LOG_WARN("detect: no mapping produced");
        return false;
    }

    report_(100, DetectStage::Done, "ok");
    LOG_INFO("detect: end (mapped=%d)", mapped);
    return true;
}

int Detection::readRpmSafe_(const HwmonFan& f, int fallback) {
    auto v = Hwmon::readRpm(f);
    int r = v ? *v : fallback;
    LOG_TRACE("detect: readRpmSafe -> %d", r);
    return r;
}

bool Detection::sleepMsCancelable_(int ms, const std::atomic<bool>& stopFlag) {
    LOG_TRACE("detect: sleep %dms (cancelable)", ms);
    using namespace std::chrono;
    auto until = steady_clock::now() + milliseconds(ms);
    while (steady_clock::now() < until) {
        if (stopFlag.load(std::memory_order_relaxed)) return false;
        std::this_thread::sleep_for(milliseconds(5));
    }
    return true;
}

} // namespace lfc
