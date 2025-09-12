#pragma once
// Header aligned to Engine.cpp expectations (per your build log)

#include <string>
#include <vector>
#include <utility>
#include <thread>
#include <atomic>
#include <mutex>

struct CurvePoint { double x{0.0}; double y{0.0}; };

struct Channel {
    std::string id;           // string id
    std::string name;
    std::string sensor_path;  // temp source
    std::string pwm_path;     // pwm sysfs
    std::string enable_path;  // pwmN_enable
    std::string tach_path;    // fanN_input
    std::string mode;         // "Auto"/"Manual"
    double      manualValue{0.0};
    double      hystC{0.0};
    double      tauS{0.0};
    std::vector<CurvePoint> curve;
    double min_pct{0.0};
    double max_pct{100.0};
};

class Engine {
public:
    // lifecycle
    void setChannels(std::vector<Channel> chs);
    std::vector<Channel> snapshot();
    bool start();
    void stop();
    bool running() const { return running_; }

    // mutations used by Daemon
    void updateChannelMode(const std::string& id, const std::string& mode);
    void updateChannelManual(const std::string& id, double pct);
    void updateChannelCurve(const std::string& id, const std::vector<CurvePoint>& pts);
    void updateChannelHystTau(const std::string& id, double hyst, double tau);

    // helpers
    static double evalCurve(const std::vector<CurvePoint>& pts, double x);

private:
    void loop();

private:
    std::vector<Channel> channels_;
    std::atomic<bool>    running_{false};
    std::thread          th_;
    std::mutex           mtx_;
    // (controller_ remains in Engine.cpp TU if you keep it there)
};
