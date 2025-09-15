#pragma once
// Control engine: channels, modes, curves, hysteresis, tick loop

#include <string>
#include <vector>
#include <unordered_map>
#include <atomic>
#include <chrono>
#include "Hwmon.hpp"
#include "ShmTelemetry.hpp"

namespace lfc {

  enum class ChannelMode { Manual=0, Curve=1 };

  struct CurvePt { double x; double y; }; // x=temp(C), y=percent(0..100)

  struct Channel {
    int id{0};
    std::string name;
    ChannelMode mode{ChannelMode::Manual};
    double manualPercent{30.0};
    double hystTauSec{2.0}; // exponential smoothing tau
    std::vector<CurvePt> curve; // must be sorted by x
    HwmonPwm pwm; // target pwm node
    double lastOut{0.0}; // smoothed output
  };

  class Engine {
  public:
    Engine() = default;

    // lifecycle
    bool initShm(const std::string& path);
    void setSnapshot(const HwmonSnapshot& s);
    void start();
    void stop();
    bool running() const { return running_; }
    void tick(); // call periodically

    // channels mgmt
    std::vector<Channel> list() const;
    int  create(const std::string& name, const HwmonPwm& pwm);
    bool remove(int id);
    bool setMode(int id, ChannelMode m);
    bool setManual(int id, double pct);
    bool setCurve(int id, std::vector<CurvePt> pts);
    bool setHystTau(int id, double tau);

    // detection
    std::string detectAggressive(int level);

  private:
    std::unordered_map<int, Channel> ch_;
    int nextId_{1};
    HwmonSnapshot snap_;
    std::atomic<bool> running_{false};
    std::chrono::steady_clock::time_point lastTick_{};
    ShmTelemetry shm_;

    static double evalCurve(const std::vector<CurvePt>& c, double x);
  };

} // namespace lfc
