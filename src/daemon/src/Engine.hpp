/*
 * Linux Fan Control — Engine (header)
 * - Periodic sensor readout and PWM control
 * - SHM telemetry writer (fixed-size buffer)
 * - Snapshot-driven device inventory
 * (c) 2025 LinuxFanControl contributors
 */
#pragma once
#include <string>
#include <chrono>

#include "Hwmon.hpp"

namespace lfc {

  class Engine {
  public:
    Engine();
    ~Engine();

    void setSnapshot(const HwmonSnapshot& snap);
    bool initShm(const std::string& path);
    void start();
    void stop();
    void tick();

  private:
    // devices snapshot
    HwmonSnapshot snap_;

    // lifecycle
    bool running_{false};
    std::chrono::steady_clock::time_point lastTick_{};

    // telemetry SHM
    std::string shmPath_;
    static constexpr std::size_t kShmSize = 64 * 1024;
    char* shmPtr_{nullptr};
  };

} // namespace lfc
