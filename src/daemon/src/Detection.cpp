/*
 * Linux Fan Control — Detection (implementation)
 * - Per-PWM mapping to candidate tach inputs (fanN in same hwmon)
 * - Spin-up check window (3s) before deciding to skip
 * - 10s total at 100% for measured PWMs, with peak RPM capture
 * - Restores original PWM duty on completion or abort
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
#include <cctype>

namespace lfc {

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

static int filename_index_suffix(const std::string& name, const char* prefix) {
    // prefix "pwm" -> "pwm3" => 3
    size_t plen = std::strlen(prefix);
    if (name.size() <= plen) return -1;
    if (name.rfind(prefix, 0) != 0) return -1;
    int v = 0;
    for (size_t i = plen; i < name.size(); ++i) {
        if (!std::isdigit(static_cast<unsigned char>(name[i]))) return -1;
        v = v * 10 + (name[i] - '0');
    }
    return v > 0 ? v : -1;
}

static std::string parent_hwmon_dir(const std::string& any_sysfs_path) {
    // e.g. /sys/class/hwmon/hwmon2/pwm1 -> /sys/class/hwmon/hwmon2
    return std::filesystem::path(any_sysfs_path).parent_path().string();
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
    // tunables
    const int spinup_check_ms = 3000;        // window to detect RPM change before skip
    const int spinup_poll_ms  = 100;         // poll interval during spinup window
    const int measure_total_ms = 10000;      // total at 100% for measured PWMs
    const int rpm_delta_thresh = 100;        // minimal RPM delta to consider "changed"

    // baseline saved duties
    for (size_t i = 0; i < snap_.pwms.size(); ++i) {
        savedDuty_[i] = read_pwm_percent_from_sysfs(snap_.pwms[i].path_pwm);
    }

    for (size_t i = 0; i < snap_.pwms.size() && !stop_.load(); ++i) {
        idx_.store(static_cast<int>(i));
        phase_ = "spinup";

        const auto& pwm = snap_.pwms[i];
        const std::string pwm_dir = parent_hwmon_dir(pwm.path_pwm);
        const int pwm_idx = filename_index_suffix(std::filesystem::path(pwm.path_pwm).filename().string(), "pwm");

        // candidate fans: same hwmon dir, prefer matching fanN; if none, fall back to all fans in same hwmon
        std::vector<const Hwmon::Fan*> cand;
        for (const auto& f : snap_.fans) {
            if (parent_hwmon_dir(f.path_input) == pwm_dir) {
                if (pwm_idx > 0) {
                    int fi = filename_index_suffix(std::filesystem::path(f.path_input).filename().string(), "fan");
                    if (fi == pwm_idx) cand.push_back(&f);
                }
            }
        }
        if (cand.empty()) {
            for (const auto& f : snap_.fans) {
                if (parent_hwmon_dir(f.path_input) == pwm_dir) cand.push_back(&f);
            }
        }

        // read baseline over candidates
        auto read_cand_max = [&]() -> int {
            int m = 0;
            for (const auto* pf : cand) {
                m = std::max(m, Hwmon::readRpm(*pf).value_or(0));
            }
            return m;
        };
        int baseline = read_cand_max();

        // drive 100% manual
        Hwmon::setManual(pwm);
        Hwmon::setPercent(pwm, 100);

        // spin-up window: wait up to spinup_check_ms for detectable RPM change on candidate fans
        bool changed = false;
        const auto t0 = std::chrono::steady_clock::now();
        while (!stop_.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(spinup_poll_ms));
            int cur = read_cand_max();
            if (cur >= baseline + rpm_delta_thresh) {
                changed = true;
                break;
            }
            if (std::chrono::steady_clock::now() - t0 >= std::chrono::milliseconds(spinup_check_ms)) break;
        }

        if (!changed) {
            // no observable tach response: restore quickly and skip this pwm
            int restore = savedDuty_[i] >= 0 ? savedDuty_[i] : 0;
            Hwmon::setPercent(pwm, restore);
            peakRpm_[i] = -1;
            LFC_LOGI("detection: pwm[%zu] skipped (no rpm change on mapped fans)", i);
            continue;
        }

        // measure window: keep 100% for remaining time to complete ~10s total
        phase_ = "measure";
        int maxRpm = read_cand_max();
        const auto t_measure_end = t0 + std::chrono::milliseconds(measure_total_ms);
        while (!stop_.load() && std::chrono::steady_clock::now() < t_measure_end) {
            std::this_thread::sleep_for(std::chrono::milliseconds(spinup_poll_ms));
            int v = read_cand_max();
            if (v > maxRpm) maxRpm = v;
        }
        peakRpm_[i] = maxRpm;
        LFC_LOGI("detection: pwm[%zu] peak_rpm=%d", i, maxRpm);

        // restore original duty
        phase_ = "restore";
        int restore = savedDuty_[i] >= 0 ? savedDuty_[i] : 0;
        Hwmon::setPercent(pwm, restore);
    }

    // best-effort restore for any saved duties
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
