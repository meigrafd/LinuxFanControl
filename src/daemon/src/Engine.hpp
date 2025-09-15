/*
 * Linux Fan Control — Engine (header)
 * - Periodic sensor readout and telemetry
 * - Optional PWM control (disabled by default)
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

    void enableControl(bool on);
    bool controlEnabled() const { return controlEnabled_; }

    bool getTelemetry(std::string& out) const;

  private:
    HwmonSnapshot snap_;

    bool running_{false};
    std::chrono::steady_clock::time_point lastTick_{};

    bool controlEnabled_{false};
    int lastPercent_{-1};

    std::string shmPath_;
    static constexpr std::size_t kShmSize = 64 * 1024;
    char* shmPtr_{nullptr};
  };

} // namespace lfc
