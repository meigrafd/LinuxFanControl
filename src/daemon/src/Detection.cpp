/*
 * Linux Fan Control — Detection (implementation)
 * - Correlates PWM outputs with tach inputs by probing
 * (c) 2025 LinuxFanControl contributors
 */
#include "Detection.hpp"
#include "Hwmon.hpp"
#include "Log.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <thread>

namespace lfc {

static bool read_int_file(const std::filesystem::path& p, int& out) {
    std::ifstream f(p);
    if (!f) return false;
    int v = 0;
    f >> v;
    if (!f.good()) return false;
    out = v;
    return true;
}
static void write_int_file(const std::filesystem::path& p, int v) {
    std::ofstream f(p);
    if (!f) return;
    f << v << "\n";
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

void Detection::abort() { stop_.store(true); }
void Detection::poll() {}

Detection::Status Detection::status() const {
    Status s;
    s.running = running_.load();
    s.currentIndex = idx_.load();
    s.total = static_cast<int>(snap_.pwms.size());
    s.phase = phase_;
    return s;
}
std::vector<int> Detection::results() const { return peakRpm_; }

void Detection::worker() {
    for (size_t i = 0; i < snap_.pwms.size(); ++i) {
        if (stop_.load()) break;
        idx_.store(static_cast<int>(i));
        phase_ = "prepare";

        const auto& pwm = snap_.pwms[i];
        const std::string pwm_dir = parent_hwmon_dir(pwm.path_pwm);
        const int pwm_idx = filename_index_suffix(std::filesystem::path(pwm.path_pwm).filename().string(), "pwm");

        // save current state
        if (savedDuty_[i] < 0) savedDuty_[i] = Hwmon::readPercent(pwm).value_or(-1);
        int en = -1; (void)read_int_file(pwm_enable_path(pwm.path_pwm), en);
        int mo = -1; (void)read_int_file(pwm_mode_path(pwm.path_pwm), mo);
        savedEnable_[i] = en;
        savedMode_[i]   = mo;

        // candidate fans in same hwmon dir, prefer matching index
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
            for (size_t k = 0; k < snap_.fans.size(); ++k)
                if (parent_hwmon_dir(snap_.fans[k].path_input) == pwm_dir) cand.push_back(k);
        }

        auto read_cand_max = [&]() -> int {
            int m = 0;
            for (size_t idx : cand) m = std::max(m, Hwmon::readRpm(snap_.fans[idx]).value_or(0));
            return m;
        };
        auto read_global_max = [&](size_t& outIdx) -> int {
            int m = 0; size_t mi = static_cast<size_t>(-1);
            for (size_t k = 0; k < snap_.fans.size(); ++k) {
                if (claimedFans_[k]) continue;
                int v = Hwmon::readRpm(snap_.fans[k]).value_or(0);
                if (v > m) { m = v; mi = k; }
            }
            outIdx = mi;
            return m;
        };

        int baseline_cand = cand.empty() ? 0 : read_cand_max();
        size_t gi = static_cast<size_t>(-1);
        int baseline_global = read_global_max(gi);

        // force manual
        write_int_file(pwm_enable_path(pwm.path_pwm), 1);

        auto do_attempt = [&](int use_mode, int& max_rpm_out) -> bool {
            if (stop_.load()) return false;
            phase_ = "mode";
            if (use_mode >= 0) write_int_file(pwm_mode_path(pwm.path_pwm), use_mode);
            std::this_thread::sleep_for(std::chrono::milliseconds(cfg_.modeDwellMs));

            phase_ = "spinup";
            Hwmon::setPercent(pwm, cfg_.rampStartPercent);
            std::this_thread::sleep_for(std::chrono::milliseconds(cfg_.settleMs));
            Hwmon::setPercent(pwm, cfg_.rampEndPercent);
            std::this_thread::sleep_for(std::chrono::milliseconds(cfg_.settleMs));

            // wait up to spinupCheckMs for delta
            const auto t0 = std::chrono::steady_clock::now();
            bool detected = false; bool via_global = false; size_t gidx = static_cast<size_t>(-1);
            while (!stop_.load()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(cfg_.spinupPollMs));
                int curC = cand.empty() ? 0 : read_cand_max();
                if (!cand.empty() && curC >= baseline_cand + cfg_.rpmDeltaThresh) { detected = true; break; }
                size_t gi2 = static_cast<size_t>(-1);
                int curG = read_global_max(gi2);
                if (curG >= baseline_global + cfg_.rpmDeltaThresh) { detected = true; via_global = true; gidx = gi2; break; }
                if (std::chrono::steady_clock::now() - t0 >= std::chrono::milliseconds(cfg_.spinupCheckMs)) break;
            }
            if (!detected) { max_rpm_out = -1; return false; }

            phase_ = "measure";
            int maxRpm = 0;
            const auto tend = std::chrono::steady_clock::now() + std::chrono::milliseconds(cfg_.measureTotalMs);
            while (!stop_.load() && std::chrono::steady_clock::now() < tend) {
                std::this_thread::sleep_for(std::chrono::milliseconds(cfg_.spinupPollMs));
                int v = 0;
                if (!via_global && !cand.empty()) v = read_cand_max();
                else if (via_global && gidx != static_cast<size_t>(-1)) v = Hwmon::readRpm(snap_.fans[gidx]).value_or(0);
                else { size_t tmp = static_cast<size_t>(-1); v = read_global_max(tmp); }
                if (v > maxRpm) maxRpm = v;
            }
            max_rpm_out = maxRpm;
            if (via_global && gidx != static_cast<size_t>(-1)) claimedFans_[gidx] = true;
            return true;
        };

        int curMode = -1; (void)read_int_file(pwm_mode_path(pwm.path_pwm), curMode);
        if (curMode < 0) curMode = 0;
        int altMode = (curMode == 0) ? 1 : 0;

        int rpm_cur = -1, rpm_alt = -1;
        bool ok_cur = do_attempt(curMode, rpm_cur);
        bool ok_alt = false;

        if (!ok_cur || rpm_cur < cfg_.rpmDeltaThresh) {
            // keep duty high to avoid audible drop before switching
            Hwmon::setPercent(pwm, cfg_.rampEndPercent);
            std::this_thread::sleep_for(std::chrono::milliseconds(cfg_.settleMs));
            ok_alt = do_attempt(altMode, rpm_alt);
        }

        phase_ = "restore";
        if (savedDuty_[i] >= 0) Hwmon::setPercent(pwm, savedDuty_[i]);
        if (savedMode_[i] >= 0) write_int_file(pwm_mode_path(pwm.path_pwm), savedMode_[i]);
        if (savedEnable_[i] >= 0) write_int_file(pwm_enable_path(pwm.path_pwm), savedEnable_[i]);

        int chosenMode = -1;
        int chosenRpm  = -1;
        if (ok_cur && rpm_cur >= rpm_alt) { chosenMode = curMode; chosenRpm = rpm_cur; }
        else if (ok_alt)                  { chosenMode = altMode; chosenRpm = rpm_alt; }

        peakRpm_[i] = chosenRpm;
        LFC_LOGI("detection: pwm[%zu] modes tried: [%d,%d] selected=%d peak_rpm=%d",
                 i, curMode, altMode, chosenMode, chosenRpm);

        phase_ = "idle";
    }

    phase_ = "done";
    running_.store(false);
}

} // namespace lfc
