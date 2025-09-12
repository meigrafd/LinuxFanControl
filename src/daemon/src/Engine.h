#pragma once
// Engine interface aligned with Engine.cpp & Daemon.cpp usage

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

    // Names match your Engine.cpp usage:
    double      manual_pct{0.0};
    double      hyst_c{0.0};
    double      tau_s{0.0};

    std::vector<CurvePoint> curve;
    double min_pct{0.0};
    double max_pct{100.0};
};

class Engine {
public:
    // lifecycle / data flow
    void setChannels(std::vector<Channel> chs);
    std::vector<Channel> snapshot();
    bool start();
    void stop();
    bool running() const { return running_; }

    // mutations used by Daemon JSON-RPC (stubs keep linking)
    int  createChannel(const std::string&, const std::string&, const std::string&) { return 0; }
    bool deleteChannel(int) { return false; }
    bool setMode(int, const std::string&) { return false; }
    bool setManual(int, double) { return false; }
    bool setCurve(int, const std::vector<std::pair<double,double>>&) { return false; }
    bool setHystTau(int, double, double) { return false; }
    bool deleteCoupling(int) { return false; }

    // optional getter if Daemon still calls channels()
    const std::vector<Channel>& channels() const { return chans_; }

    // helpers
    static double evalCurve(const std::vector<CurvePoint>& pts, double x);

private:
    void loop();

private:
    std::vector<Channel> chans_;   // name matches Engine.cpp
    std::atomic<bool>    running_{false};
    std::thread          th_;
    std::mutex           mu_;      // name matches Engine.cpp
    // controller_ remains where you define it
};
