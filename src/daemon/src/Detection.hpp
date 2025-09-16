/*
 * Linux Fan Control — Detection (header)
 * Fan/curve detection logic and status reporting.
 * (c) 2025 LinuxFanControl contributors
 */
#pragma once

#include <vector>
#include <string>
#include <atomic>
#include <cstdint>
#include <chrono>
#include <thread>      // <-- needed for std::thread

#include "Hwmon.hpp"  // provides HwmonInventory

namespace lfc {

// Treat "HwmonSnapshot" as our enumerated inventory.
using HwmonSnapshot = HwmonInventory;

// Tuning parameters (deine ursprünglichen Felder)
struct DetectionConfig {
    int settleMs{250};
    int spinupCheckMs{5000};
    int spinupPollMs{100};
    int measureTotalMs{10000};
    int rpmDeltaThresh{30};
    int rampStartPercent{30};
    int rampEndPercent{100};
};

struct DetectionStatus {
    bool running { false };
    int  currentIndex { 0 };
    int  total { 0 };
    int  phase { 0 }; // implementation-defined
};

class Detection {
public:
    Detection(const HwmonSnapshot& snap, const DetectionConfig& cfg);

    // non-copyable
    Detection(const Detection&) = delete;
    Detection& operator=(const Detection&) = delete;

    // lifecycle
    void start();
    void abort();
    bool running() const;

    // polling / status
    void poll();
    DetectionStatus status() const;

    // results (e.g., detected max RPM per PWM index)
    std::vector<int> results() const;

private:
    // internal helpers
    void worker();
    void restoreOriginals();

private:
    HwmonSnapshot  snap_;
    DetectionConfig cfg_;

    std::thread thr_;
    std::atomic<bool> running_{false};
    std::atomic<bool> stop_{false};

    // progress
    std::atomic<int> currentIndex_{0};
    int total_{0};
    std::atomic<int> phase_{0};

    // saved state per PWM (best effort)
    struct PwmOrig {
        std::string path_pwm;
        std::string path_enable;
        int enableVal{2}; // default auto
        int rawVal{0};
    };
    std::vector<PwmOrig> orig_;

    // results buffer
    std::vector<int> maxRpm_;
};

} // namespace lfc
