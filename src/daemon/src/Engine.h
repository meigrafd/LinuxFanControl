#pragma once
// Minimal Engine surface so Daemon can link; real logic remains in your implementation.

#include <string>
#include <vector>
#include <utility>

struct Channel {
    int         id{0};
    std::string name;
    std::string sensorPath;
    std::string pwmPath;
    std::string enablePath;
    std::string tachPath;
    std::string mode;       // "Auto" / "Manual"
    double      manualValue{0.0};
    double      hystC{0.0};
    double      tauS{0.0};
    std::vector<std::pair<double,double>> curve;
    double min_pct{0.0};
    double max_pct{100.0};
};

class Engine {
public:
    void start();
    void stop();
    bool running() const;

    // Keep daemon building; replace with your real implementations later.
    int  createChannel(const std::string&, const std::string&, const std::string&) { return 0; }
    bool deleteChannel(int) { return false; }
    bool setMode(int, const std::string&) { return false; }
    bool setManual(int, double) { return false; }
    bool setCurve(int, const std::vector<std::pair<double,double>>&) { return false; }
    bool setHystTau(int, double, double) { return false; }
    bool deleteCoupling(int) { return false; }
    const std::vector<Channel>& channels() const {
        static const std::vector<Channel> empty; return empty;
    }
};
