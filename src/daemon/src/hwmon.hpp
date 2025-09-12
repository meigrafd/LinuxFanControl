#pragma once
// All comments in English by request.

#include <string>
#include <vector>
#include <optional>
#include <utility>

// ---------- Model types ----------

struct TempSensor {
    std::string label;     // e.g. "hwmonX:chip:CPU Tctl" or "libsensors:<chip>:<label>"
    std::string path;      // sysfs path OR "ls://<chip>#<feature_number>"
    std::string type;      // CPU/GPU/NVMe/Water/Ambient/...
    std::string unit;      // "°C"
};

struct PwmDevice {
    std::string label;                 // "hwmonX:chip:pwmN"
    std::string pwm_path;              // /sys/class/hwmon/hwmonX/pwmN
    std::optional<std::string> enable_path; // .../pwmN_enable
    std::optional<std::string> tach_path;   // .../fanN_input (optional)
    bool writable = false;             // result of probe
};

// ---------- Control (curves + filter) ----------

struct Curve {
    // (x=temp °C, y=duty %)
    std::vector<std::pair<double,double>> pts;
    double eval(double x) const; // linear segments, clamped 0..100
};

struct SchmittSlew {
    double hyst_c = 0.0;   // hysteresis width in °C
    double tau_s  = 0.0;   // response time constant (first-order) in seconds

    // mutable state
    mutable double last_y = 0.0;
    mutable std::optional<double> last_x;
    mutable bool   dir_up  = true;
    mutable double t_last  = 0.0;

    // returns duty %
    double step(const Curve& c, double x, double now_s) const;
};

struct Channel {
    std::string id;              // stable id
    std::string name;            // display name
    std::string sensor_path;     // sysfs path or ls://...
    std::string pwm_path;        // output duty path
    std::optional<std::string> enable_path;
    Curve        curve;
    SchmittSlew  filter;
    std::string  mode = "Auto";  // "Auto" | "Manual"
    double       manual_pct = 0.0;
    // telemetry
    double last_temp = 0.0;
    double last_out  = 0.0;
};

// ---------- IO helpers ----------

std::vector<TempSensor> enumerate_temps(); // sysfs + libsensors
std::vector<PwmDevice>  enumerate_pwms();  // sysfs

double read_temp_c(const std::string& path);  // supports "ls://"
int    read_rpm(const std::string& path);
bool   set_pwm_enable(const std::string& enable_path, int mode); // 0=off,1=manual,2=auto
bool   write_pwm_pct(const std::string& pwm_path, double pct);   // 0..100

bool   read_file_str(const std::string& path, std::string& out);
bool   write_file_str(const std::string& path, const std::string& s);

// probe if pwm is writable (try enable=1, no-op write & restore)
bool   probe_pwm_writable(PwmDevice &dev, std::string &reason);

// ---------- Calibration & Coupling detection ----------

struct CalibResult {
    bool ok=false;
    int  spinup_pct=0;
    int  min_pct=0;
    std::string error;
};

// 1) Boost to 100% for hold, 2) step down to find min stable (rpm >= threshold). Restores snapshot.
CalibResult calibrate_pwm(const PwmDevice& dev,
                          int rpm_threshold=100,
                          int floor_pct=20,
                          int step=5,
                          double settle_s=1.0,
                          double boost_hold_s=10.0);

struct CoupleRec { std::string sensor_path; std::string sensor_label; double score=0.0; };
std::vector<std::pair<std::string, CoupleRec>>
detect_coupling(const std::vector<PwmDevice>& pwms,
                const std::vector<TempSensor>& temps,
                double hold_s=10.0,
                double min_delta_c=1.0,
                int rpm_delta_threshold=80);
