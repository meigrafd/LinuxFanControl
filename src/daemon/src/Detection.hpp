/*
 * Linux Fan Control — Detection (header)
 * - Non-blocking fan detection worker
 * - Saves/restores original PWM state
 * - Collects peak RPMs per PWM
 * (c) 2025 LinuxFanControl contributors
 */
#pragma once
#include <thread>
#include <atomic>
#include <string>
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
    bool running() const { return running_.load(); }
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
    std::vector<int> savedDuty_;
    std::vector<int> peakRpm_;
};

} // namespace lfc
