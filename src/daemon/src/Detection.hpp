/*
 * Linux Fan Control — Detection (header)
 * - Non-blocking PWM-to-fan detection worker
 * - Saves/restores pwmN_enable and duty
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

class Detection {
public:
    struct Status {
        bool running{false};
        int currentIndex{0};
        int total{0};
        std::string phase;
    };

    explicit Detection(const HwmonSnapshot& snap);
    ~Detection();

    void start();
    void abort();
    void poll();

    Status status() const;
    std::vector<int> results() const;

private:
    void worker();

private:
    HwmonSnapshot snap_;

    std::thread thr_;
    std::atomic<bool> running_{false};
    std::atomic<bool> stop_{false};

    std::atomic<int> idx_{0};
    std::string phase_;

    // saved state per PWM
    std::vector<int> savedDuty_;
    std::vector<int> savedEnable_;      // <- added

    // results
    std::vector<int> peakRpm_;

    // global fan-claiming to avoid duplicate matches
    std::vector<bool> claimedFans_;     // <- added
};

} // namespace lfc
