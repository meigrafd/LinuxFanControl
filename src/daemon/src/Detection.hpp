/*
 * Linux Fan Control — Detection (header)
 * - Non-blocking PWM-to-fan detection worker
 * - Saves/restores pwmN_enable, pwmN_mode and duty
 * - Captures peak RPM per PWM
 * (c) 2025 LinuxFanControl contributors
 */
#pragma once

#include <atomic>
#include <string>
#include <thread>
#include <vector>

#include "Hwmon.hpp"

namespace lfc {

struct DetectionConfig {
    int settleMs{250};
    int spinupCheckMs{5000};
    int spinupPollMs{100};
    int measureTotalMs{10000};
    int rpmDeltaThresh{30};
    int rampStartPercent{50};
    int rampEndPercent{100};
};

class Detection {
public:
    struct Status {
        bool running{false};
        int currentIndex{0};
        int total{0};
        std::string phase;
    };

    Detection(const HwmonSnapshot& snap, const DetectionConfig& cfg);
    ~Detection();

    void start();
    void abort();
    void poll();

    Status status() const;
    std::vector<int> results() const;

    bool running() const { return running_.load(); }

private:
    void worker();

    HwmonSnapshot snap_;
    DetectionConfig cfg_;

    std::thread thr_;
    std::atomic<bool> running_{false};
    std::atomic<bool> stop_{false};

    std::atomic<int> idx_{0};
    std::string phase_;

    // saved state per PWM
    std::vector<int> savedDuty_;
    std::vector<int> savedEnable_;
    std::vector<int> savedMode_;

    // results
    std::vector<int> peakRpm_;

    // avoid matching same tach twice globally
    std::vector<bool> claimedFans_;
};

} // namespace lfc
