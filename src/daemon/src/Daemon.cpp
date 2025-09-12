/*
 * Linux Fan Control (LFC) - Daemon main & RPC handlers
 * (c) 2025 meigrafd & contributors - MIT License (see LICENSE)
 *
 * This file wires the JSON-RPC server with engine/setup routines.
 * Key point: detectCalibrate never aborts on first error; it aggregates results.
 */

#include "Hwmon.h"
#include "RpcServer.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <thread>
#include <utility>
#include <vector>

// ---------- Small helpers ----------
static inline std::string now_iso() {
    using namespace std::chrono;
    auto t = system_clock::now();
    std::time_t tt = system_clock::to_time_t(t);
    char buf[64];
    std::strftime(buf, sizeof(buf), "%FT%TZ", std::gmtime(&tt));
    return std::string(buf);
}

static inline std::optional<std::string> read_all_opt(const std::string& p) {
    std::ifstream f(p);
    if (!f.good()) return std::nullopt;
    std::string s; std::getline(f, s);
    return s;
}

static inline bool write_all(const std::string& p, const std::string& s) {
    std::ofstream f(p);
    if (!f.good()) return false;
    f << s;
    return f.good();
}

struct Snapshot {
    std::string enable;
    std::string pwm;
    bool has_enable = false;
    bool has_pwm = false;
};

static Snapshot take_snapshot(const Hwmon& hw, const PwmDevice& d) {
    (void)hw;
    Snapshot s{};
    if (!d.enable_path.empty()) {
        if (auto v = read_all_opt(d.enable_path)) {
            s.enable = *v; s.has_enable = true;
        }
    }
    if (!d.pwm_path.empty()) {
        if (auto v = read_all_opt(d.pwm_path)) {
            s.pwm = *v; s.has_pwm = true;
        }
    }
    return s;
}

static void restore_snapshot(const PwmDevice& d, const Snapshot& s) {
    // best-effort restore; ignore individual failures
    if (s.has_enable) (void)write_all(d.enable_path, s.enable);
    if (s.has_pwm)    (void)write_all(d.pwm_path,    s.pwm);
}

// Probe whether a PWM looks writable without bricking behavior.
static bool probe_pwm_writable(const PwmDevice& d, std::string* why) {
    // Check basic file presence
    std::ifstream test(d.pwm_path);
    if (!test.good()) { if (why) *why = "pwm not readable"; return false; }

    // Try to set manual mode if possible (ignore failure)
    if (!d.enable_path.empty()) {
        std::ofstream en(d.enable_path);
        if (!en.good()) {
            // Some drivers don’t support it at all; keep going to test a write below.
        } else {
            en << "1";
        }
    }

    // Try a no-op write of current value
    auto before = read_all_opt(d.pwm_path);
    if (!before) { if (why) *why = "pwm read failed"; return false; }

    // Write exactly the same content back; check kernel errno (EOPNOTSUPP/EBUSY common)
    std::ofstream w(d.pwm_path);
    if (!w.good()) { if (why) *why = "pwm open(O_WRONLY) failed"; return false; }
    w << *before;
    if (!w.good())  { if (why) *why = "pwm write failed"; return false; }

    return true;
}

// ---------- Daemon class ----------
class Daemon {
public:
    explicit Daemon(bool debug) : debug_(debug) {}

    // RPC: list sensors
    nlohmann::json listSensors() {
        Hwmon hw;
        auto temps = hw.discoverTemps();
        nlohmann::json arr = nlohmann::json::array();
        for (const auto& t : temps) {
            arr.push_back({
                {"name",  t.name},
                {"label", t.label},
                {"path",  t.path},
                {"type",  t.type} // may be "Unknown"
            });
        }
        return arr;
    }

    // RPC: list PWMs
    nlohmann::json listPwms() {
        Hwmon hw;
        auto pwms = hw.discoverPwms();
        nlohmann::json arr = nlohmann::json::array();
        for (const auto& p : pwms) {
            arr.push_back({
                {"label",        p.label},
                {"pwm_path",     p.pwm_path},
                {"enable_path",  p.enable_path},
                {"tach_path",    p.tach_path},
                {"min_pct",      p.min_pct},
                {"max_pct",      p.max_pct}
            });
        }
        return arr;
    }

    // RPC: detect + calibrate (non-fatal on errors, returns full report)
    nlohmann::json detectCalibrate() {
        Hwmon hw;

        // 1) Discover
        auto temps = hw.discoverTemps();
        auto pwms  = hw.discoverPwms();

        // 2) Snapshots
        std::map<std::string, Snapshot> snaps;
        for (const auto& d : pwms) {
            snaps[d.label] = take_snapshot(hw, d);
        }

        // 3) Probe writability and build skip list (do NOT abort entire run)
        std::set<std::string> skipped;
        nlohmann::json errors = nlohmann::json::array();

        for (const auto& d : pwms) {
            std::string why;
            if (!probe_pwm_writable(d, &why)) {
                skipped.insert(d.label);
                errors.push_back({
                    {"device", d.label},
                    {"stage",  "probe"},
                    {"error",  why}
                });
            }
        }

        // 4) Basic active coupling pulse (safe floor 20%, boost 100% 5s) – quick heuristic
        //    We only touch devices not skipped; failures are recorded and we continue.
        const double floor_pct = 20.0;
        const double boost_pct = 100.0;
        const auto   dwell_ms  = std::chrono::milliseconds(5000);

        // Index temps by path for quick sampling
        auto read_all_temps = [&]() {
            nlohmann::json vals = nlohmann::json::object();
            for (const auto& t : temps) {
                double v = Hwmon::readTempC(t.path);
                if (!std::isnan(v)) vals[t.path] = v;
            }
            return vals;
        };

        // We accumulate a simple “delta” metric for each pwm->sensor candidate.
        std::map<std::string, std::pair<std::string,double>> best; // pwm_label -> (sensor_path, score)

        for (const auto& d : pwms) {
            if (skipped.count(d.label)) continue;

            // get baseline
            auto base = read_all_temps();

            // floor
            {
                std::string err;
                if (!Hwmon::setPwmPercent(d, floor_pct, &err)) {
                    errors.push_back({{"device", d.label}, {"stage","set_floor"}, {"error", err}});
                    skipped.insert(d.label);
                    continue;
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            auto low = read_all_temps();

            // boost
            {
                std::string err;
                if (!Hwmon::setPwmPercent(d, boost_pct, &err)) {
                    errors.push_back({{"device", d.label}, {"stage","set_boost"}, {"error", err}});
                    skipped.insert(d.label);
                    continue;
                }
            }
            std::this_thread::sleep_for(dwell_ms);
            auto high = read_all_temps();

            // compute simple deltas |low-base| and |high-base| → choose the largest absolute change
            double bestScore = 0.0;
            std::string bestPath;
            for (auto it = base.begin(); it != base.end(); ++it) {
                const std::string path = it.key();
                if (!low.contains(path) || !high.contains(path)) continue;
                const double b = base[path].get<double>();
                const double l = low[path].get<double>();
                const double h = high[path].get<double>();
                // more weight on boost phase
                double score = std::abs(h - b) * 2.0 + std::abs(l - b);
                if (score > bestScore) { bestScore = score; bestPath = path; }
            }

            if (!bestPath.empty()) {
                best[d.label] = {bestPath, bestScore};
            }
        }

        // 5) Calibrate min (spin) quickly (non-fatal)
        nlohmann::json calibration = nlohmann::json::array();
        for (const auto& d : pwms) {
            if (skipped.count(d.label)) continue;

            // quick downwards sweep (never below floor)
            const int start = 40, end = 20, step = 5;
            int min_stable = start;
            bool had_error = false;

            for (int pct = start; pct >= end; pct -= step) {
                std::string err;
                if (!Hwmon::setPwmPercent(d, (double)pct, &err)) {
                    errors.push_back({{"device", d.label}, {"stage","calibrate"}, {"error", err}});
                    had_error = true;
                    break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
                // if we have tach, check threshold
                if (!d.tach_path.empty()) {
                    auto sv = read_all_opt(d.tach_path);
                    if (sv && !sv->empty()) {
                        long rpm = std::strtol(sv->c_str(), nullptr, 10);
                        if (rpm > 100) min_stable = pct; // very rough threshold
                        else break;
                    }
                } else {
                    // no tach => keep configured floor
                    min_stable = end;
                }
            }

            calibration.push_back({{"device", d.label}, {"min_pct", min_stable}});
        }

        // 6) Restore snapshots regardless of above results
        for (const auto& d : pwms) restore_snapshot(d, snaps[d.label]);

        // 7) Build mapping result in readable form
        nlohmann::json mapping = nlohmann::json::array();
        for (const auto& d : pwms) {
            if (!best.count(d.label)) continue;
            const auto& pair = best[d.label];
            // find human label for sensor path
            std::string sensorLabel = pair.first;
            std::string sensorType  = "Unknown";
            for (const auto& t : temps) {
                if (t.path == pair.first) {
                    sensorLabel = t.label.empty() ? t.path : (t.type.empty() ? t.label : (t.type + ": " + t.label));
                    sensorType  = t.type;
                    break;
                }
            }
            mapping.push_back({
                {"pwm",          d.label},
                {"sensor_path",  pair.first},
                {"sensor_label", sensorLabel},
                {"sensor_type",  sensorType},
                {"score",        pair.second}
            });
        }

        // 8) Final JSON
        nlohmann::json out = {
            {"timestamp",  now_iso()},
            {"sensors",    listSensors()},
            {"pwms",       listPwms()},
            {"mapping",    mapping},
            {"calibration",calibration},
            {"skipped",    nlohmann::json::array()},
            {"errors",     errors}
        };
        for (const auto& s : skipped) out["skipped"].push_back(s);
        return out;
    }

private:
    bool debug_ = false;
};

// ---------- main & RPC wiring ----------
int main(int argc, char** argv) {
    bool debug = false;
    bool foreground = false;
    for (int i=1;i<argc;i++) {
        if (std::strcmp(argv[i], "--debug")==0) debug = true;
        else if (std::strcmp(argv[i], "--foreground")==0) foreground = true;
    }

    Daemon daemon(debug);
    RpcServer rpc(/*debug=*/debug);

    // JSON-RPC 2.0 handlers
    rpc.on("listSensors",     [&] (const nlohmann::json&) { return daemon.listSensors(); });
    rpc.on("listPwms",        [&] (const nlohmann::json&) { return daemon.listPwms(); });
    rpc.on("detectCalibrate", [&] (const nlohmann::json&) { return daemon.detectCalibrate(); });

    if (!foreground) {
        // TODO: optional daemonize here if you want background mode
    }

    return rpc.run(); // blocks; handles stdin/stdout JSON-RPC
}
