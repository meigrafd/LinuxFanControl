// Daemon.cpp — JSON-RPC 2.0 daemon with batch support and SHM telemetry.
// Control path: JSON-RPC (batch). Data path: shared memory ring.
// Comments in English per project guideline.

#include "Daemon.h"
#include "Hwmon.h"
#include "Engine.h"

#include <sys/socket.h>
#include <sys/un.h>
#include <sys/poll.h>
#include <unistd.h>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <vector>
#include <thread>
#include <chrono>
#include <fstream>
#include <algorithm>
#include <cmath>
#include <map>
#include <atomic>

#include <nlohmann/json.hpp>
#include "common/ShmTelemetry.h"

using json = nlohmann::json;
using namespace std::chrono_literals;

// -------------------- small helpers --------------------
static std::string getenv_or(const char* k, const char* defv) {
    const char* v = std::getenv(k);
    return v ? std::string(v) : std::string(defv);
}

static bool write_all(int fd, const void* buf, size_t n) {
    const char* p = static_cast<const char*>(buf);
    size_t left = n;
    while (left > 0) {
        ssize_t w = ::write(fd, p, left);
        if (w < 0) {
            if (errno == EINTR) continue;
            return false;
        }
        left -= static_cast<size_t>(w);
        p += w;
    }
    return true;
}

static bool read_line(int fd, std::string& out) {
    out.clear();
    char ch;
    while (true) {
        ssize_t r = ::read(fd, &ch, 1);
        if (r == 0) return false; // EOF
        if (r < 0) {
            if (errno == EINTR) continue;
            return false;
        }
        if (ch == '\n') break;
        out.push_back(ch);
        if (out.size() > (1u << 20)) return false; // 1MB guard
    }
    return true;
}

static bool try_connect(const std::string& sockPath) {
    int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return false;
    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    std::snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", sockPath.c_str());
    bool ok = (::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0);
    if (ok) ::shutdown(fd, SHUT_RDWR);
    ::close(fd);
    return ok;
}

static inline double clamp(double v, double lo, double hi) {
    return std::max(lo, std::min(hi, v));
}
static inline double milli_to_c(double v) {
    return (v > 200.0 ? v / 1000.0 : v);
}

// -------------------- Telemetry publisher (SHM) --------------------
class TelemetryPublisher {
public:
    TelemetryPublisher() = default;
    ~TelemetryPublisher() { stop(); }

    bool start(Engine* engine, const std::string& shmName, uint32_t capacity = 4096, int period_ms = 200) {
        stop();
        engine_ = engine;
        periodMs_ = period_ms;
        if (!lfc::shm::createOrOpen(map_, shmName.c_str(), capacity, /*create*/true)) {
            std::fprintf(stderr, "[daemon] SHM create failed: %s\n", shmName.c_str());
            return false;
        }
        running_.store(true);
        thr_ = std::thread([this]() { this->run(); });
        return true;
    }

    void stop() {
        running_.store(false);
        if (thr_.joinable()) thr_.join();
        lfc::shm::destroy(map_);
    }

private:
    void run() {
        while (running_.load()) {
            // Pull latest snapshot and publish a frame per channel
            auto chs = engine_->snapshot();
            const uint64_t ts = std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count();
                for (const auto& c : chs) {
                    lfc::shm::writeFrame(map_, c.id.c_str(), c.last_out, c.last_temp, ts);
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(periodMs_));
        }
    }

    Engine* engine_{nullptr};
    int periodMs_{200};
    std::atomic<bool> running_{false};
    std::thread thr_;
    lfc::shm::Mapping map_;
};

// -------------------- lifecycle ------------------------
Daemon::Daemon()
: sockPath_(getenv_or("LFC_SOCK", "/tmp/lfcd.sock"))
, srvFd_(-1)
, running_(false)
, debug_(std::getenv("LFC_DEBUG") != nullptr)
, hw_(new Hwmon())
, engine_(new Engine())
{}

Daemon::~Daemon() {
    shutdown();
    delete engine_;
    delete hw_;
}

bool Daemon::isAlreadyRunning() const {
    return try_connect(sockPath_);
}

static TelemetryPublisher g_pub; // single publisher instance

bool Daemon::init() {
    if (running_) return true;

    if (isAlreadyRunning()) {
        std::fprintf(stderr, "[daemon] another instance is already running at %s\n", sockPath_.c_str());
        return false;
    }

    ::unlink(sockPath_.c_str());

    srvFd_ = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (srvFd_ < 0) {
        std::fprintf(stderr, "[daemon] socket() failed: %s\n", std::strerror(errno));
        return false;
    }

    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    std::snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", sockPath_.c_str());

    if (::bind(srvFd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        std::fprintf(stderr, "[daemon] bind(%s) failed: %s\n", sockPath_.c_str(), std::strerror(errno));
        ::close(srvFd_); srvFd_ = -1;
        return false;
    }
    if (::listen(srvFd_, 64) < 0) {
        std::fprintf(stderr, "[daemon] listen() failed: %s\n", std::strerror(errno));
        ::close(srvFd_); srvFd_ = -1;
        return false;
    }

    // Init SHM publisher (engine is not running yet; publisher will still publish last snapshot)
    const std::string shmName = getenv_or("LFC_SHM", "/lfc_telemetry");
    (void)g_pub.start(engine_, shmName, 4096, 200);

    running_ = true;
    std::fprintf(stderr, "[daemon] listening on %s\n", sockPath_.c_str());
    return true;
}

bool Daemon::pumpOnce(int timeoutMs) {
    if (!running_) return false;
    if (srvFd_ < 0) return false;

    struct pollfd pfd{};
    pfd.fd = srvFd_;
    pfd.events = POLLIN;
    int pr = ::poll(&pfd, 1, timeoutMs);
    if (pr < 0) {
        if (errno == EINTR) return true;
        std::fprintf(stderr, "[daemon] poll() failed: %s\n", std::strerror(errno));
        return false;
    }
    if (pr == 0) return true;

    int cfd = ::accept4(srvFd_, nullptr, nullptr, 0);
    if (cfd < 0) {
        if (errno == EINTR) return true;
        std::fprintf(stderr, "[daemon] accept() failed: %s\n", std::strerror(errno));
        return true;
    }

    std::string line;
    bool have = read_line(cfd, line);
    if (!have) { ::close(cfd); return true; }

    if (debug_) std::fprintf(stderr, "[daemon] RX: %s\n", line.c_str());

    json reply;
    try {
        json req = json::parse(line);
        if (req.is_array()) {
            json arr = json::array();
            for (auto& one : req) {
                json r = dispatch(one);
                if (r.contains("id")) arr.push_back(std::move(r));
            }
            reply = arr;
        } else {
            reply = dispatch(req);
        }
    } catch (const std::exception& e) {
        reply = error_obj(nullptr, -32700, std::string("Parse error: ") + e.what());
    }

    const std::string payload = reply.dump() + "\n";
    if (debug_) std::fprintf(stderr, "[daemon] TX: %s", payload.c_str());
    write_all(cfd, payload.data(), payload.size());
    ::close(cfd);
    return true;
}

void Daemon::shutdown() {
    if (!running_) return;
    running_ = false;
    g_pub.stop();
    if (srvFd_ >= 0) {
        ::shutdown(srvFd_, SHUT_RDWR);
        ::close(srvFd_);
        srvFd_ = -1;
    }
    ::unlink(sockPath_.c_str());
}

// -------------------- RPC dispatch (unchanged control path) ---------------------
json Daemon::dispatch(const json& req) {
    if (!req.is_object()) return error_obj(nullptr, -32600, "Invalid Request");

    const auto itJ = req.find("jsonrpc");
    const auto itM = req.find("method");
    const auto itI = req.find("id");
    const auto itP = req.find("params");

    json id = (itI == req.end() ? json() : *itI);
    if (itJ == req.end() || !itJ->is_string() || itJ->get<std::string>() != "2.0")
        return error_obj(id, -32600, "Invalid Request");
    if (itM == req.end() || !itM->is_string())
        return error_obj(id, -32600, "Invalid Request");

    const std::string method = itM->get<std::string>();
    const json params = (itP == req.end() ? json::object() : *itP);

    try {
        if (method == "ping")         return result_obj(id, json{{"pong", true}});
        if (method == "version")      return result_obj(id, json{{"name","lfcd"},{"protocol","jsonrpc2-batch"},{"version","1.1"}});
        if (method == "enumerate")    return result_obj(id, rpcEnumerate());
        if (method == "listChannels") return result_obj(id, rpcListChannels());
        if (method == "detectCalibrate") return result_obj(id, rpcDetectCalibrate());

        if (method == "createChannel") {
            const std::string name   = params.value("name",   "");
            const std::string sensor = params.value("sensor", "");
            const std::string pwm    = params.value("pwm",    "");
            return result_obj(id, json{{"ok", rpcCreateChannel(name, sensor, pwm)}});
        }
        if (method == "deleteChannel") {
            const std::string cid = params.value("id", "");
            return result_obj(id, json{{"ok", rpcDeleteChannel(cid)}});
        }
        if (method == "setChannelMode") {
            const std::string cid = params.value("id", "");
            const std::string md  = params.value("mode","Auto");
            return result_obj(id, json{{"ok", rpcSetChannelMode(cid, md)}});
        }
        if (method == "setChannelManual") {
            const std::string cid = params.value("id", "");
            const double pct = params.value("pct", 0.0);
            return result_obj(id, json{{"ok", rpcSetChannelManual(cid, pct)}});
        }
        if (method == "setChannelCurve") {
            const std::string cid = params.value("id", "");
            std::vector<std::pair<double,double>> pts;
            if (params.contains("points") && params["points"].is_array()) {
                for (auto& p : params["points"]) {
                    if (p.is_array() && p.size()==2) {
                        pts.emplace_back(p[0].get<double>(), p[1].get<double>());
                    } else if (p.is_object() && p.contains("x") && p.contains("y")) {
                        pts.emplace_back(p["x"].get<double>(), p["y"].get<double>());
                    }
                }
            }
            return result_obj(id, json{{"ok", rpcSetChannelCurve(cid, pts)}});
        }
        if (method == "setChannelHystTau") {
            const std::string cid = params.value("id", "");
            const double hyst = params.value("hyst", 0.0);
            const double tau  = params.value("tau",  0.0);
            return result_obj(id, json{{"ok", rpcSetChannelHystTau(cid, hyst, tau)}});
        }
        if (method == "engineStart") {
            bool ok = engine_->start();
            return result_obj(id, json{{"ok", ok}});
        }
        if (method == "engineStop") {
            engine_->stop();
            return result_obj(id, json{{"ok", true}});
        }
        if (method == "deleteCoupling") {
            const std::string cid = params.value("id", "");
            return result_obj(id, json{{"ok", rpcDeleteCoupling(cid)}});
        }

        return error_obj(id, -32601, "Method not found");
    } catch (const std::exception& e) {
        return error_obj(id, -32603, std::string("Internal error: ") + e.what());
    }
}

// -------------------- RPC impls ------------------------
json Daemon::rpcEnumerate() {
    auto temps = hw_->discoverTemps();
    auto pwms  = hw_->discoverPwms();

    json jt = json::array();
    for (auto& t : temps) {
        jt.push_back(json{
            {"name",  t.name},
            {"label", t.label},
            {"path",  t.path},
            {"type",  t.type}
        });
    }

    json jp = json::array();
    for (auto& p : pwms) {
        std::string label = p.pwm_path;
        auto pos = label.rfind('/');
        if (pos != std::string::npos) {
            auto base = label.substr(pos+1); // pwmN
            auto dir  = label.substr(0, pos);
            auto pos2 = dir.rfind('/');
            if (pos2 != std::string::npos) dir = dir.substr(pos2+1);
            label = dir + ":" + base;
        }
        jp.push_back(json{
            {"label",  label},
            {"pwm",    p.pwm_path},
            {"enable", p.enable_path},
            {"tach",   p.tach_path}
        });
    }

    return json{{"sensors", jt}, {"pwms", jp}};
}

json Daemon::rpcListChannels() {
    const auto chs = engine_->snapshot();
    json arr = json::array();
    for (const auto& c : chs) {
        json pts = json::array();
        for (auto& pt : c.curve) pts.push_back({pt.x, pt.y});
        arr.push_back(json{
            {"id",     c.id},
            {"name",   c.name},
            {"sensor", c.sensor_path},
            {"pwm",    c.pwm_path},
            {"enable", c.enable_path},
            {"tach",   c.tach_path},
            {"mode",   c.mode},
            {"manual", c.manual_pct},
            {"hyst",   c.hyst_c},
            {"tau",    c.tau_s},
            {"curve",  pts},
            {"last_out",  c.last_out},
            {"last_temp", c.last_temp}
        });
    }
    return arr;
}

bool Daemon::rpcCreateChannel(const std::string& name,
                              const std::string& sensor,
                              const std::string& pwm) {
    auto chs = engine_->snapshot();
    Channel c;
    c.id = std::to_string(static_cast<unsigned long long>(
        std::chrono::steady_clock::now().time_since_epoch().count()));
    c.name = name;
    c.sensor_path = sensor;
    c.pwm_path = pwm;
    c.mode = "Auto";
    c.curve = { {20,20}, {40,40}, {60,60}, {80,100} };
    chs.push_back(std::move(c));
    engine_->setChannels(std::move(chs));
    return true;
}

bool Daemon::rpcDeleteChannel(const std::string& id) {
    if (id.empty()) return false;
    auto chs = engine_->snapshot();
    const auto n0 = chs.size();
    chs.erase(std::remove_if(chs.begin(), chs.end(),
                            [&](const Channel& c){ return c.id == id; }),
            chs.end());
    if (chs.size() == n0) return false;
    engine_->setChannels(std::move(chs));
    return true;
}

bool Daemon::rpcSetChannelMode(const std::string& id, const std::string& mode) {
    engine_->updateChannelMode(id, mode);
    return true;
}

bool Daemon::rpcSetChannelManual(const std::string& id, double pct) {
    engine_->updateChannelManual(id, pct);
    return true;
}

bool Daemon::rpcSetChannelCurve(const std::string& id,
                                const std::vector<std::pair<double,double>>& pts) {
    std::vector<CurvePoint> v;
    v.reserve(pts.size());
    for (auto& p : pts) v.push_back(CurvePoint{p.first, p.second});
    engine_->updateChannelCurve(id, v);
    return true;
}

bool Daemon::rpcSetChannelHystTau(const std::string& id, double hyst, double tau) {
    engine_->updateChannelHystTau(id, hyst, tau);
    return true;
}

bool Daemon::rpcDeleteCoupling(const std::string& /*id*/) {
    return true;
}

// -------------------- detect + calibrate ----------------

// Safe read helpers
static inline bool read_text(const std::string& path, std::string& out) {
    out.clear();
    std::ifstream f(path);
    if (!f) return false;
    std::getline(f, out);
    return true;
}
static inline bool write_text(const std::string& path, const std::string& val) {
    std::ofstream f(path);
    if (!f) return false;
    f << val;
    return static_cast<bool>(f);
}
static inline bool write_pwm_pct(const std::string& path, double pct) {
    int raw = static_cast<int>(std::round(clamp(pct, 0.0, 100.0) * 255.0 / 100.0));
    return write_text(path, std::to_string(raw));
}
static inline double pct_from_raw(const std::string& raw) {
    try { int x = std::stoi(raw); if (x<0) return NAN; return clamp(x * 100.0 / 255.0, 0.0, 100.0); }
    catch (...) { return NAN; }
}
static inline int read_rpm(const std::string& tachPath) {
    std::ifstream f(tachPath);
    if (!f) return -1;
    int v=0; f>>v; return f ? v : -1;
}
static inline double read_temp(const std::string& path) {
    std::ifstream f(path);
    if (!f) return NAN;
    double v=0; f>>v; return f ? milli_to_c(v) : NAN;
}

json Daemon::rpcDetectCalibrate() {
    const int floor_pct = 20;
    const int delta_pct = 25;      // strong step so audible
    const double dwell_s = 10.0;   // longer dwell for clear response
    const int repeats = 1;
    const int rpm_threshold = 100;
    const int step = 5;
    const double settle_s = 1.0;

    auto temps = hw_->discoverTemps();
    auto pwms  = hw_->discoverPwms();

    // snapshot enable/pwm
    struct Snap { std::string enable; std::string pwm; bool has_en=false, has_pwm=false; };
    std::map<std::string, Snap> snaps; // by label

    auto label_of_pwm = [](const std::string& pwm_path) {
        std::string label = pwm_path;
        auto pos = label.rfind('/');
        if (pos != std::string::npos) {
            auto base = label.substr(pos+1);
            auto dir  = label.substr(0, pos);
            auto pos2 = dir.rfind('/');
            if (pos2 != std::string::npos) dir = dir.substr(pos2+1);
            label = dir + ":" + base;
        }
        return label;
    };

    // temp map path->value
    auto read_all_temps = [&](std::map<std::string,double>& out) {
        out.clear();
        for (auto& t : temps) {
            double v = read_temp(t.path);
            if (std::isfinite(v)) out[t.path] = v;
        }
    };

    // Build results
    json jSensors = json::array();
    for (auto& t : temps) {
        jSensors.push_back(json{
            {"name", t.name}, {"label", t.label}, {"path", t.path}, {"type", t.type}
        });
    }
    json jPwms = json::array();
    for (auto& p : pwms) {
        jPwms.push_back(json{
            {"label", label_of_pwm(p.pwm_path)},
                        {"pwm",   p.pwm_path},
                        {"enable",p.enable_path},
                        {"tach",  p.tach_path}
        });
    }

    // Coupling inference
    json mapping = json::object();
    for (auto& p : pwms) {
        const std::string lbl = label_of_pwm(p.pwm_path);

        // snapshot
        Snap s{};
        if (!p.enable_path.empty()) { s.has_en = read_text(p.enable_path, s.enable); }
        s.has_pwm = read_text(p.pwm_path, s.pwm);
        snaps[lbl] = s;

        // try set manual if enable exists
        if (p.enable_path.size()) {
            // write "1", but don't bail if not supported
            write_text(p.enable_path, "1");
        }

        std::string prev_raw;
        read_text(p.pwm_path, prev_raw);
        double prev_pct = pct_from_raw(prev_raw);
        if (!std::isfinite(prev_pct)) prev_pct = 35.0;
        double start_pct = std::max<double>(floor_pct, prev_pct);

        // write lower bound (never below floor)
        auto safe_write = [&](double pct) -> bool {
            return write_pwm_pct(p.pwm_path, std::max<double>(floor_pct, pct));
        };

        // If writing is unsupported (e.g. amdgpu Errno 95), skip but keep snapshot restore
        bool write_ok = safe_write(start_pct); // initial write to test FS
        if (!write_ok) {
            if (debug_) std::fprintf(stderr, "[detect] skip %s: write not supported\n", p.pwm_path.c_str());
            continue;
        }

        std::map<std::string,double> t_low, t_base;
        for (int rep = 0; rep < repeats; ++rep) {
            double low = std::max<double>(floor_pct, start_pct - std::abs(delta_pct));
            (void)safe_write(low);
            std::this_thread::sleep_for(std::chrono::milliseconds(int(dwell_s * 1000)));
            read_all_temps(t_low);

            (void)safe_write(start_pct);
            std::this_thread::sleep_for(std::chrono::milliseconds(int(dwell_s * 1000)));
            read_all_temps(t_base);
        }

        // score by |low - base|
        double best_v = 0.0;
        std::string best_path;
        for (auto& t : temps) {
            auto itL = t_low.find(t.path);
            auto itB = t_base.find(t.path);
            if (itL!=t_low.end() && itB!=t_base.end()) {
                double sc = std::fabs(itL->second - itB->second);
                if (sc > best_v) { best_v = sc; best_path = t.path; }
            }
        }
        if (!best_path.empty()) {
            // label lookup for sensor
            std::string sLabel = best_path;
            for (auto& t : temps) if (t.path==best_path) { sLabel = t.label; break; }
            mapping[lbl] = json{{"sensor_label", sLabel}, {"sensor_path", best_path}, {"score", best_v}};
        }

        // restore to start
        (void)safe_write(start_pct);
        // restore enable + raw if we had snapshot
        if (s.has_en) write_text(p.enable_path, s.enable);
        if (s.has_pwm) write_text(p.pwm_path, s.pwm);
    }

    // Calibration
    json cal_res = json::object();
    for (auto& p : pwms) {
        const std::string lbl = label_of_pwm(p.pwm_path);
        const auto itS = snaps.find(lbl);
        const int floorHere = floor_pct;

        // set manual if possible
        if (!p.enable_path.empty()) write_text(p.enable_path, "1");

        // graceful PWM test
        std::string prev_raw2;
        read_text(p.pwm_path, prev_raw2);
        double prev_pct2 = pct_from_raw(prev_raw2);
        if (!std::isfinite(prev_pct2)) prev_pct2 = 35.0;

        auto safe_write2 = [&](double pct) -> bool {
            return write_pwm_pct(p.pwm_path, std::max<double>(floorHere, pct));
        };
        if (!safe_write2(prev_pct2)) {
            cal_res[lbl] = json{{"ok", false}, {"error","PWM write not supported"}};
            // restore snapshot if any
            if (itS!=snaps.end()) {
                if (itS->second.has_en) write_text(p.enable_path, itS->second.enable);
                if (itS->second.has_pwm) write_text(p.pwm_path, itS->second.pwm);
            }
            continue;
        }

        int spinup = -1;
        int rpm_at_min = 0;

        for (int duty = std::max(0, floorHere); duty <= 100; duty += step) {
            (void)safe_write2(duty);
            std::this_thread::sleep_for(std::chrono::milliseconds(int(settle_s * 1000)));
            int rpm = (p.tach_path.empty() ? -1 : read_rpm(p.tach_path));
            if (rpm >= rpm_threshold) { spinup = duty; rpm_at_min = rpm; break; }
        }
        if (spinup < 0) {
            cal_res[lbl] = json{{"ok", false}, {"error","No spin detected"}, {"min_pct", 100}};
            // restore
            if (itS!=snaps.end()) {
                if (itS->second.has_en) write_text(p.enable_path, itS->second.enable);
                if (itS->second.has_pwm) write_text(p.pwm_path, itS->second.pwm);
            }
            continue;
        }

        int min_stable = spinup;
        for (int duty = spinup; duty >= floorHere; duty -= step) {
            (void)safe_write2(duty);
            std::this_thread::sleep_for(std::chrono::milliseconds(int(settle_s * 1000)));
            int rpm = (p.tach_path.empty() ? -1 : read_rpm(p.tach_path));
            if (rpm >= rpm_threshold) {
                min_stable = duty;
            } else break;
        }

        cal_res[lbl] = json{{"ok", true}, {"spinup_pct", spinup}, {"min_pct", min_stable}, {"rpm_at_min", rpm_at_min}};

        // restore snapshot
        if (itS!=snaps.end()) {
            if (itS->second.has_en) write_text(p.enable_path, itS->second.enable);
            if (itS->second.has_pwm) write_text(p.pwm_path, itS->second.pwm);
        }
    }

    return json{{"sensors", jSensors}, {"pwms", jPwms}, {"mapping", mapping}, {"cal_res", cal_res}};
}

// -------------------- JSON helpers ---------------------
json Daemon::error_obj(const json& id, int code, const std::string& msg) {
    json r = { {"jsonrpc","2.0"}, {"error", {{"code", code}, {"message", msg}}} };
    if (!id.is_null()) r["id"] = id;
    return r;
}

json Daemon::result_obj(const json& id, const json& result) {
    json r = { {"jsonrpc","2.0"}, {"result", result} };
    if (!id.is_null()) r["id"] = id;
    return r;
}
