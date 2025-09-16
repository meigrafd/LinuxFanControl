/*
 * Linux Fan Control — Detection (implementation)
 * - Save/restore pwmN_enable, pwmN_mode and duty
 * - Per-PWM candidate fans (same hwmon) with global fallback
 * - Spin-up 7s, 30 RPM delta, ramp 50%→100%, 10s measure
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
#include <cstring>
#include <vector>

namespace lfc {

static bool read_int_file(const std::filesystem::path& p, int& out) {
    std::ifstream f(p);
    if (!f) return false;
    int v = 0;
    if (!(f >> v)) return false;
    out = v;
    return true;
}

static void write_int_file(const std::filesystem::path& p, int v) {
    std::ofstream f(p);
    if (!f) return;
    f << v << "\n";
}

static int read_pwm_percent_from_sysfs(const std::string& path_pwm) {
    std::filesystem::path p(path_pwm);
    int maxv = 255;
    {
        std::filesystem::path pmax = p.parent_path() / (p.filename().string() + "_max");
        int tmp = 0;
        if (read_int_file(pmax, tmp) && tmp > 0) {
            maxv = tmp;
        }
    }
    int raw = 0;
    if (read_int_file(p, raw) && raw >= 0) {
        if (maxv <= 0) maxv = 255;
        int pct = static_cast<int>(std::lround((100.0 * raw) / maxv));
        if (pct < 0) pct = 0;
        if (pct > 100) pct = 100;
        return pct;
    }
    return -1;
}

static int filename_index_suffix(const std::string& name, const char* prefix) {
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
    return std::filesystem::path(any_sysfs_path).parent_path().string();
}

static std::filesystem::path pwm_enable_path(const std::string& path_pwm) {
    auto p = std::filesystem::path(path_pwm);
    return p.parent_path() / (p.filename().string() + "_enable");
}

static std::filesystem::path pwm_mode_path(const std::string& path_pwm) {
    auto p = std::filesystem::path(path_pwm);
    return p.parent_path() / (p.filename().string() + "_mode");
}

Detection::Detection(const HwmonSnapshot& snap, const DetectionConfig& cfg)
    : snap_(snap), cfg_(cfg) {}

Detection::~Detection() {
    abort();
    if (thr_.joinable()) thr_.join();
}

void Detection::start() {
    if (running_.exchange(true)) return;
    stop_.store(false);
    idx_.store(0);
    phase_.clear();
    savedDuty_.assign(snap_.pwms.size(), -1);
    savedEnable_.assign(snap_.pwms.size(), -1);
    savedMode_.assign(snap_.pwms.size(), -1);
    peakRpm_.assign(snap_.pwms.size(), -1);
    claimedFans_.assign(snap_.fans.size(), false);
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
    const int cfg_.settleMs = 250;
    const int cfg_.spinupCheckMs = 7000;
    const int cfg_.spinupPollMs  = 100;
    const int cfg_.measureTotalMs = 10000;
    const int cfg_.rpmDeltaThresh = 30;

    for (size_t i = 0; i < snap_.pwms.size(); ++i) {
        savedDuty_[i] = read_pwm_percent_from_sysfs(snap_.pwms[i].path_pwm);
        int en = -1; read_int_file(pwm_enable_path(snap_.pwms[i].path_pwm), en);
        int mo = -1; read_int_file(pwm_mode_path(snap_.pwms[i].path_pwm), mo);
        savedEnable_[i] = en;
        savedMode_[i] = mo;
    }

    auto read_global_max = [&](const std::vector<bool>& claimed) -> std::pair<int,size_t> {
        int m = 0;
        size_t mi = static_cast<size_t>(-1);
        for (size_t k = 0; k < snap_.fans.size(); ++k) {
            if (claimed[k]) continue;
            int v = Hwmon::readRpm(snap_.fans[k]).value_or(0);
            if (v > m) { m = v; mi = k; }
        }
        return {m, mi};
    };

    for (size_t i = 0; i < snap_.pwms.size() && !stop_.load(); ++i) {
        idx_.store(static_cast<int>(i));
        phase_ = "spinup";

        const auto& pwm = snap_.pwms[i];
        const std::string pwm_dir = parent_hwmon_dir(pwm.path_pwm);
        const int pwm_idx = filename_index_suffix(std::filesystem::path(pwm.path_pwm).filename().string(), "pwm");

        std::vector<size_t> cand;
        for (size_t k = 0; k < snap_.fans.size(); ++k) {
            if (parent_hwmon_dir(snap_.fans[k].path_input) == pwm_dir) {
                if (pwm_idx > 0) {
                    int fi = filename_index_suffix(std::filesystem::path(snap_.fans[k].path_input).filename().string(), "fan");
                    if (fi == pwm_idx) cand.push_back(k);
                }
            }
        }
        if (cand.empty()) {
            for (size_t k = 0; k < snap_.fans.size(); ++k) {
                if (parent_hwmon_dir(snap_.fans[k].path_input) == pwm_dir) cand.push_back(k);
            }
        }

        auto read_cand_max = [&]() -> int {
            int m = 0;
            for (size_t idx : cand) m = std::max(m, Hwmon::readRpm(snap_.fans[idx]).value_or(0));
            return m;
        };

        int baseline_cand = cand.empty() ? 0 : read_cand_max();
        int baseline_global = read_global_max(claimedFans_).first;

        int prevEn = -1, prevMode = -1;
        (void)read_int_file(pwm_enable_path(pwm.path_pwm), prevEn);
        (void)read_int_file(pwm_mode_path(pwm.path_pwm), prevMode);
        if (prevEn >= 0) savedEnable_[i] = prevEn;
        if (prevMode >= 0) savedMode_[i] = prevMode;

        write_int_file(pwm_enable_path(pwm.path_pwm), 1);

        auto attempt = [&](int use_mode, int measure_ms, bool& detected, bool& via_global, size_t& global_idx, int& max_rpm_out) {
            detected = false; via_global = false; global_idx = static_cast<size_t>(-1); max_rpm_out = 0;
            if (use_mode >= 0) write_int_file(pwm_mode_path(pwm.path_pwm), use_mode);
            std::this_thread::sleep_for(std::chrono::milliseconds(cfg_.settleMs));
            Hwmon::setPercent(pwm, cfg_.rampStartPercent);
            std::this_thread::sleep_for(std::chrono::milliseconds(cfg_.settleMs));
            Hwmon::setPercent(pwm, cfg_.rampEndPercent);
            std::this_thread::sleep_for(std::chrono::milliseconds(cfg_.settleMs));

            const auto t0 = std::chrono::steady_clock::now();
            while (!stop_.load()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(cfg_.spinupPollMs));
                if (!cand.empty()) {
                    int cur = read_cand_max();
                    if (cur >= baseline_cand + cfg_.rpmDeltaThresh) { detected = true; break; }
                }
                auto gm = read_global_max(claimedFans_);
                if (gm.first >= baseline_global + cfg_.rpmDeltaThresh) { detected = true; via_global = true; global_idx = gm.second; break; }
                if (std::chrono::steady_clock::now() - t0 >= std::chrono::milliseconds(cfg_.spinupCheckMs)) break;
            }
            if (!detected) return;

            phase_ = "measure";
            int maxRpm = 0;
            const auto t_end = t0 + std::chrono::milliseconds(measure_ms);
            while (!stop_.load() && std::chrono::steady_clock::now() < t_end) {
                std::this_thread::sleep_for(std::chrono::milliseconds(cfg_.spinupPollMs));
                int v = 0;
                if (!via_global && !cand.empty()) v = read_cand_max();
                else if (via_global && global_idx != static_cast<size_t>(-1)) v = Hwmon::readRpm(snap_.fans[global_idx]).value_or(0);
                else v = read_global_max(claimedFans_).first;
                if (v > maxRpm) maxRpm = v;
            }
            max_rpm_out = maxRpm;
        };

        bool det = false, via_global = false;
        size_t gidx = static_cast<size_t>(-1);
        int measured_max = 0;

        int curMode = -1; (void)read_int_file(pwm_mode_path(pwm.path_pwm), curMode);
        int altMode = (curMode == 0) ? 1 : 0;

        attempt(curMode, cfg_.measureTotalMs, det, via_global, gidx, measured_max);
        if (!det) {
            LFC_LOGD("detection: pwm[%zu] no change in mode=%d, trying mode=%d", i, curMode, altMode);
            attempt(altMode, cfg_.measureTotalMs, det, via_global, gidx, measured_max);
        }

        phase_ = "restore";
        int restoreDuty = savedDuty_[i] >= 0 ? savedDuty_[i] : 0;
        Hwmon::setPercent(pwm, restoreDuty);
        if (savedMode_[i] >= 0) write_int_file(pwm_mode_path(pwm.path_pwm), savedMode_[i]);
        if (savedEnable_[i] >= 0) write_int_file(pwm_enable_path(pwm.path_pwm), savedEnable_[i]);

        if (!det) {
            peakRpm_[i] = -1;
            LFC_LOGI("detection: pwm[%zu] skipped (no rpm change in both modes)", i);
            continue;
        }

        peakRpm_[i] = measured_max;
        if (via_global && gidx != static_cast<size_t>(-1)) claimedFans_[gidx] = true;
        LFC_LOGI("detection: pwm[%zu] peak_rpm=%d (mode tried: %d/%d)", i, measured_max, curMode, altMode);
    }

    phase_ = "restore_all";
    for (size_t i = 0; i < snap_.pwms.size(); ++i) {
        if (savedDuty_[i] >= 0) Hwmon::setPercent(snap_.pwms[i], savedDuty_[i]);
        if (savedMode_[i] >= 0) write_int_file(pwm_mode_path(snap_.pwms[i].path_pwm), savedMode_[i]);
        if (savedEnable_[i] >= 0) write_int_file(pwm_enable_path(snap_.pwms[i].path_pwm), savedEnable_[i]);
    }

    phase_ = "done";
    running_.store(false);
}

} // namespace lfc
