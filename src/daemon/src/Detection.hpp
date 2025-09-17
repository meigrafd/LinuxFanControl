/*
 * Linux Fan Control — Detection (header)
 * - Non-blocking PWM→fan detection worker
 * (c) 2025 LinuxFanControl contributors
 */
#pragma once

#include <string>
#include <vector>
#include <atomic>
#include <thread>

#include "Hwmon.hpp"  // HwmonInventory, Hwmon{Temp,Fan,Pwm}

namespace lfc {

using HwmonSnapshot = HwmonInventory;

struct DetectionConfig {
    int settleMs{250};
    int spinupCheckMs{5000};
    int spinupPollMs{100};
    int measureTotalMs{10000};
    int rpmDeltaThresh{30};
    int rampStartPercent{30};
    int rampEndPercent{100};
    int modeDwellMs{600};   // wait after writing pwm*_mode to avoid audible drop
};

class Detection {
public:
    struct Status {
        bool   running{false};
        int    currentIndex{0};
        int    total{0};
        std::string phase;
    };

    Detection(const HwmonSnapshot& snap, const DetectionConfig& cfg);
    ~Detection();

    void start();
    void abort();
    void poll();

    Status status() const;
    std::vector<int> results() const;

    inline bool running() const { return running_.load(); }

private:
    void worker();

private:
    HwmonSnapshot  snap_;
    DetectionConfig cfg_;

    std::atomic<bool> running_{false};
    std::atomic<bool> stop_{false};
    std::thread       thr_;

    std::atomic<int>  idx_{0};
    std::string       phase_;

    std::vector<int>  savedDuty_;
    std::vector<int>  savedEnable_;
    std::vector<int>  savedMode_;
    std::vector<int>  peakRpm_;
    std::vector<bool> claimedFans_;
};

} // namespace lfc
