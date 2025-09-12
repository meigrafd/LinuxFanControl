#pragma once
// Engine interface and data model used by the daemon.
// Comments in English per project preference.

#include <string>
#include <vector>
#include <utility>
#include <thread>
#include <atomic>
#include <mutex>
#include <cstddef>

struct CurvePoint { double x{0.0}; double y{0.0}; };

struct Channel {
    std::string id;           // string id (stable)
    std::string name;

    // I/O bindings
    std::string sensor_path;  // temp source (/sys/class/hwmon/.../tempN_input)
    std::string pwm_path;     // pwm sysfs (/sys/.../pwmN)
    std::string enable_path;  // pwmN_enable
    std::string tach_path;    // fanN_input

    // Mode & parameters
    std::string mode;         // "Auto" / "Manual"
    double      manual_pct{0.0};
    double      hyst_c{0.0};
    double      tau_s{0.0};
    std::vector<CurvePoint> curve;
    double min_pct{0.0};
    double max_pct{100.0};

    // Telemetry (for UI)
    double last_temp{std::numeric_limits<double>::quiet_NaN()};
    double last_out{std::numeric_limits<double>::quiet_NaN()};
};

class Engine {
public:
    // lifecycle / data flow
    void setChannels(std::vector<Channel> chs);
    std::vector<Channel> snapshot();
    bool start();
    void stop();
    bool running() const { return running_; }

    // mutations used by Daemon / RPC
    void updateChannelMode (const std::string& id, const std::string& mode);
    void updateChannelManual(const std::string& id, double pct);
    void updateChannelCurve (const std::string& id, const std::vector<CurvePoint>& pts);
    void updateChannelHystTau(const std::string& id, double hyst, double tau);

    // helper
    static double evalCurve(const std::vector<CurvePoint>& pts, double x);

private:
    void loop();

private:
    std::vector<Channel> chans_;
    std::atomic<bool>    running_{false};
    std::thread          th_;
    std::mutex           mu_;
};
