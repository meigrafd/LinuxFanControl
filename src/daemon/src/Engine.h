#pragma once
#include "Hwmon.h"
#include <vector>
#include <string>
#include <thread>
#include <mutex>
#include <atomic>
#include <optional>

/* Simple control engine: applies channel duty by either Manual or Auto(curve) */
struct CurvePoint { double x, y; }; // x=temp, y=duty
struct Channel {
    std::string id;
    std::string name;
    std::string sensor_path;
    std::string pwm_path;
    std::string enable_path;
    std::string mode = "Auto"; // "Auto" | "Manual"
    double manual_pct = 0.0;
    double hyst_c = 0.5;
    double tau_s  = 2.0;
    std::vector<CurvePoint> curve{{20,0},{35,25},{50,50},{70,80}};
    // telemetry
    double last_temp = 0.0;
    double last_out  = 0.0;
};

class Engine {
public:
    Engine() = default;
    ~Engine() { stop(); }

    void setChannels(std::vector<Channel> chs);
    std::vector<Channel> snapshot();

    bool start();
    void stop();
    bool running() const { return running_; }

    // Immediate updates:
    void updateChannelMode(const std::string& id, const std::string& mode);
    void updateChannelManual(const std::string& id, double pct);
    void updateChannelCurve(const std::string& id, const std::vector<CurvePoint>& pts);
    void updateChannelHystTau(const std::string& id, double hyst, double tau);

private:
    std::vector<Channel> chans_;
    std::thread th_;
    std::mutex mu_;
    std::atomic<bool> running_{false};
    void loop();
    double evalCurve(const std::vector<CurvePoint>& pts, double x);
};
