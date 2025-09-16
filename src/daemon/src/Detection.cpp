/*
 * Linux Fan Control — Detection (implementation)
 * Non-blocking PWM detection worker. Saves/restores pwm*_enable and duty.
 * (c) 2025 LinuxFanControl contributors
 */
#include "Detection.hpp"
#include "Hwmon.hpp"
#include "Log.hpp"

#include <thread>
#include <chrono>
#include <algorithm>

namespace lfc {

Detection::Detection(const HwmonSnapshot& snap, const DetectionConfig& cfg)
    : snap_(snap), cfg_(cfg) {
    total_ = static_cast<int>(snap_.pwms.size());
    maxRpm_.assign(total_, 0);

    // capture originals (best effort)
    orig_.clear();
    orig_.reserve(snap_.pwms.size());
    for (const auto& p : snap_.pwms) {
        PwmOrig o;
        o.path_pwm    = p.path_pwm;
        o.path_enable = p.path_enable;
        o.enableVal   = Hwmon::readEnable(p).value_or(2);
        o.rawVal      = Hwmon::readRaw(p).value_or(0);
        orig_.push_back(o);
    }
}

void Detection::start() {
    if (running_.load()) return;
    stop_.store(false);
    running_.store(true);
    thr_ = std::thread(&Detection::worker, this);
}

void Detection::abort() {
    stop_.store(true);
    if (thr_.joinable()) thr_.join();
    restoreOriginals();
    running_.store(false);
}

bool Detection::running() const {
    return running_.load();
}

void Detection::poll() {
    // lightweight — echte Messlogik läuft im worker()
}

DetectionStatus Detection::status() const {
    DetectionStatus s{};
    s.running      = running_.load();
    s.currentIndex = currentIndex_.load();
    s.total        = total_;
    s.phase        = phase_.load();
    return s;
}

std::vector<int> Detection::results() const {
    return maxRpm_;
}

void Detection::restoreOriginals() {
    for (const auto& o : orig_) {
        Hwmon::writeRawPath(o.path_pwm, o.rawVal);
        Hwmon::writeEnable(o.path_enable, o.enableVal);
    }
}

void Detection::worker() {
    using namespace std::chrono;
    const int startPct = std::clamp(cfg_.rampStartPercent, 0, 100);
    const int endPct   = std::clamp(cfg_.rampEndPercent,   0, 100);
    const int stepPct  = std::max(1, (endPct - startPct) / 10);

    // enable manual mode on all PWMs we touch (best effort)
    for (size_t i=0;i<snap_.pwms.size();++i) {
        Hwmon::writeEnable(snap_.pwms[i], 1);
    }

    for (size_t i=0; i<snap_.pwms.size() && !stop_.load(); ++i) {
        currentIndex_.store(static_cast<int>(i));
        phase_.store(1); // spinup/ramp

        // simple ramp and track peak RPM
        int peak = 0;
        for (int pct = startPct; pct <= endPct && !stop_.load(); pct += stepPct) {
            Hwmon::setPercent(snap_.pwms[i], pct);
            std::this_thread::sleep_for(milliseconds(std::max(1, cfg_.spinupPollMs)));
            // pick a fan tach heuristically (same index if available)
            int tach = 0;
            if (i < snap_.fans.size()) {
                tach = Hwmon::readRpm(snap_.fans[i]).value_or(0);
            } else if (!snap_.fans.empty()) {
                tach = Hwmon::readRpm(snap_.fans[0]).value_or(0);
            }
            peak = std::max(peak, tach);
        }
        maxRpm_[i] = peak;

        phase_.store(2); // settle between channels
        std::this_thread::sleep_for(milliseconds(std::max(1, cfg_.settleMs)));
    }

    // restore everything
    restoreOriginals();

    running_.store(false);
    phase_.store(0);
}

} // namespace lfc
