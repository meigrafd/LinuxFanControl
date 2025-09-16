/*
 * Linux Fan Control — Detection (implementation)
 * - 10s 100% spin test per fan; skip if no RPM change within 2s
 * - Restores original PWM duty at the end or on abort
 * - Stores peak RPM per PWM for RPC retrieval
 * (c) 2025 LinuxFanControl contributors
 */
#include "Detection.hpp"
#include "Hwmon.hpp"
#include "Log.hpp"

#include <chrono>
#include <thread>
#include <algorithm>
#include <fstream>
#include <filesystem>
#include <cmath>

namespace lfc {

// Best-effort: read current PWM duty from sysfs and convert to percent (0..100).
static int read_pwm_percent_from_sysfs(const std::string& path_pwm) {
    std::filesystem::path p(path_pwm);
    int maxv = 255;
    {
        std::filesystem::path pmax = p.parent_path() / (p.filename().string() + "_max");
        std::ifstream fm(pmax);
        if (fm) {
            int tmp = 0;
            if (fm >> tmp && tmp > 0) maxv = tmp;
        }
    }
    int raw = 0;
    std::ifstream f(path_pwm);
    if (f && (f >> raw) && raw >= 0) {
        if (maxv <= 0) maxv = 255;
        int pct = static_cast<int>(std::lround((100.0 * raw) / maxv));
        if (pct < 0) pct = 0;
        if (pct > 100) pct = 100;
        return pct;
    }
    return -1;
}

Detection::Detection(const HwmonSnapshot& snap) : snap_(snap) {}

Detection::~Detection() {
    abort();
    if (thr_.joinable()) {
        thr_.join();
    }
}

void Detection::start() {
    if (running_.exchange(true)) {
        return;
    }
    stop_.store(false);
    idx_.store(0);
    phase_ = "prepare";
    savedDuty_.assign(snap_.pwms.size(), -1);
    peakRpm_.assign(snap_.pwms.size(), -1);
    thr_ = std::thread(&Detection::worker, this);
}

void Detection::abort() {
    stop_.store(true);
}

void Detection::poll() {
}

Detection::Status Detection::status() const {
    Status s;
    s.running = running_.load();
    s.currentIndex = idx_.load();
    s.total = static_cast<int>(snap_.pwms.size());
    s.phase = phase_;
    return s;
}

std::vector<int> Detection::results() const {
    return peakRpm_;
}

void Detection::worker() {
    for (size_t i = 0; i < snap_.pwms.size(); ++i) {
        savedDuty_[i] = read_pwm_percent_from_sysfs(snap_.pwms[i].path_pwm);
    }

    for (size_t i = 0; i < snap_.pwms.size() && !stop_.load(); ++i) {
        idx_.store(static_cast<int>(i));
        phase_ = "spinup";

        const auto& pwm = snap_.pwms[i];

        int baseline = 0;
        for (const auto& f : snap_.fans) {
            baseline = std::max(baseline, Hwmon::readRpm(f).value_or(0));
        }

        Hwmon::setManual(pwm);
        Hwmon::setPercent(pwm, 100);

        for (int t = 0; t < 20 && !stop_.load(); ++t) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        int peak = 0;
        for (const auto& f : snap_.fans) {
            peak = std::max(peak, Hwmon::readRpm(f).value_or(0));
        }

        if (peak <= baseline + 50) {
            LFC_LOGI("detection: pwm[%zu] skipped (no rpm change)", i);
            Hwmon::setPercent(pwm, savedDuty_[i] >= 0 ? savedDuty_[i] : 0);
            peakRpm_[i] = -1;
            continue;
        }

        phase_ = "measure";
        int maxRpm = peak;
        for (int t = 0; t < 80 && !stop_.load(); ++t) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            for (const auto& f : snap_.fans) {
                int v = Hwmon::readRpm(f).value_or(0);
                if (v > maxRpm) {
                    maxRpm = v;
                }
            }
        }
        peakRpm_[i] = maxRpm;
        LFC_LOGI("detection: pwm[%zu] peak_rpm=%d", i, maxRpm);

        phase_ = "restore";
        int restore = savedDuty_[i] >= 0 ? savedDuty_[i] : 0;
        Hwmon::setPercent(pwm, restore);
    }

    phase_ = "restore_all";
    for (size_t i = 0; i < snap_.pwms.size(); ++i) {
        if (savedDuty_[i] >= 0) {
            Hwmon::setPercent(snap_.pwms[i], savedDuty_[i]);
        }
    }

    phase_ = "done";
    running_.store(false);
}

} // namespace lfc
