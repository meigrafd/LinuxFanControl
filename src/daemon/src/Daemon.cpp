// Daemon.cpp — JSON-RPC 2.0 daemon with batch support over Unix domain socket.
// Matches main.cpp expectations: init(), pumpOnce(int), shutdown().

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

#include <nlohmann/json.hpp>

using json = nlohmann::json;

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

// -------------------- lifecycle ------------------------
Daemon::Daemon()
: sockPath_(getenv_or("LFC_SOCK", "/tmp/lfcd.sock"))
, srvFd_(-1)
, running_(false)
, hw_(new Hwmon())
, engine_(new Engine()) {
}

Daemon::~Daemon() {
    shutdown();
    delete engine_;
    delete hw_;
}

bool Daemon::init() {
    if (running_) return true;

    // remove stale socket
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

    running_ = true;
    std::fprintf(stderr, "[daemon] listening on %s\n", sockPath_.c_str());
    return true;
}

bool Daemon::pumpOnce(int timeoutMs) {
    if (!running_) return false;
    if (srvFd_ < 0) return false;

    // poll with timeout
    struct pollfd pfd{};
    pfd.fd = srvFd_;
    pfd.events = POLLIN;
    int pr = ::poll(&pfd, 1, timeoutMs);
    if (pr < 0) {
        if (errno == EINTR) return true; // keep running
        std::fprintf(stderr, "[daemon] poll() failed: %s\n", std::strerror(errno));
        return false;
    }
    if (pr == 0) {
        // timeout: nothing to accept this round
        return true;
    }

    int cfd = ::accept4(srvFd_, nullptr, nullptr, 0);
    if (cfd < 0) {
        if (errno == EINTR) return true;
        std::fprintf(stderr, "[daemon] accept() failed: %s\n", std::strerror(errno));
        return true; // keep server alive
    }

    // read exactly one line of JSON (object or array for batch)
    std::string line;
    bool have = read_line(cfd, line);
    if (!have) { ::close(cfd); return true; }

    json reply;
    try {
        json req = json::parse(line);
        if (req.is_array()) {
            json arr = json::array();
            for (auto& one : req) {
                json r = dispatch(one);
                if (r.contains("id")) arr.push_back(std::move(r)); // notifications => no "id"
            }
            reply = arr;
        } else {
            reply = dispatch(req);
        }
    } catch (const std::exception& e) {
        reply = error_obj(nullptr, -32700, std::string("Parse error: ") + e.what());
    }

    const std::string payload = reply.dump() + "\n";
    write_all(cfd, payload.data(), payload.size());
    ::close(cfd);
    return true;
}

void Daemon::shutdown() {
    if (!running_) return;
    running_ = false;
    if (srvFd_ >= 0) {
        ::shutdown(srvFd_, SHUT_RDWR);
        ::close(srvFd_);
        srvFd_ = -1;
    }
    ::unlink(sockPath_.c_str());
}

// -------------------- RPC dispatch ---------------------
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
        if (method == "ping")        return result_obj(id, json{{"pong", true}});
        if (method == "version")     return result_obj(id, json{{"name","lfcd"},{"protocol","jsonrpc2-batch"},{"version","1.0"}});
        if (method == "enumerate")   return result_obj(id, rpcEnumerate());
        if (method == "listChannels")return result_obj(id, rpcListChannels());

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
            return result_obj(id, json{{"ok", engine_->start()}});
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
        // form "<hwmonX>:<pwmN>" label
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
            {"curve",  pts}
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
    // placeholder: depends on your internal model; return true for now
    return true;
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
