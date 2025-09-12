// src/daemon/src/Daemon.cpp
// Simple JSON-RPC 2.0 daemon over a Unix domain socket (newline-delimited JSON).
// - Supports batch requests (JSON array).
// - Uses Hwmon to enumerate sensors/PWMs.
// - Uses Engine to hold channel configuration and to run control loop.
// - Comments in English per project guideline.

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <fcntl.h>

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>

#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <optional>
#include <mutex>
#include <chrono>
#include <algorithm>

#include <nlohmann/json.hpp>

#include "Hwmon.h"
#include "Engine.h"

using json = nlohmann::json;

// ----------------------------- helpers ---------------------------------

static std::string getenv_or(const char* key, const char* defv) {
    const char* v = std::getenv(key);
    return v ? std::string(v) : std::string(defv);
}

static int make_server_socket(const std::string& path) {
    // Remove stale socket
    ::unlink(path.c_str());

    int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return -1;

    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    std::snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", path.c_str());

    if (::bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        ::close(fd);
        return -1;
    }
    if (::listen(fd, 64) < 0) {
        ::close(fd);
        return -1;
    }
    return fd;
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
        if (r == 0) return false;         // EOF
        if (r < 0) {
            if (errno == EINTR) continue;
            return false;
        }
        if (ch == '\n') break;
        out.push_back(ch);
        if (out.size() > (1u<<20)) return false; // guard: 1MB per line
    }
    return true;
}

// ---------------------------- Daemon -----------------------------------

class Daemon {
public:
    explicit Daemon(std::string sock = getenv_or("LFC_SOCK", "/tmp/lfcd.sock"))
    : sockPath_(std::move(sock)) {}

    ~Daemon() { stop(); }

    bool start() {
        if (running_) return true;
        srv_ = make_server_socket(sockPath_);
        if (srv_ < 0) {
            std::fprintf(stderr, "[daemon] failed to bind %s: %s\n",
                         sockPath_.c_str(), std::strerror(errno));
            return false;
        }
        running_ = true;
        th_ = std::thread([this]{ this->acceptLoop(); });
        return true;
    }

    void stop() {
        running_ = false;
        if (srv_ >= 0) {
            ::shutdown(srv_, SHUT_RDWR);
            ::close(srv_);
            srv_ = -1;
        }
        if (th_.joinable()) th_.join();
        ::unlink(sockPath_.c_str());
    }

private:
    // ---------- RPC dispatch ----------
    json dispatch(const json& req) {
        // JSON-RPC 2.0 object: {jsonrpc:"2.0", method, id?, params?}
        if (!req.is_object()) return error_obj(nullptr, -32600, "Invalid Request");

        const auto itJ = req.find("jsonrpc");
        const auto itM = req.find("method");
        const auto itI = req.find("id");
        const auto itP = req.find("params");

        const json id = (itI == req.end() ? json() : *itI);

        if (itJ == req.end() || !itJ->is_string() || itJ->get<std::string>() != "2.0") {
            return error_obj(id, -32600, "Invalid Request");
        }
        if (itM == req.end() || !itM->is_string()) {
            return error_obj(id, -32600, "Invalid Request");
        }
        const std::string method = itM->get<std::string>();
        const json params = (itP == req.end() ? json::object() : *itP);

        try {
            if (method == "ping") {
                return result_obj(id, json{{"pong", true}});
            }
            if (method == "version") {
                return result_obj(id, json{
                    {"name", "Linux Fan Control Daemon"},
                    {"version", "1.0"},
                    {"protocol", "jsonrpc2-batch"}
                });
            }
            if (method == "enumerate") {
                return result_obj(id, rpcEnumerate());
            }
            if (method == "listChannels") {
                return result_obj(id, rpcListChannels());
            }
            if (method == "createChannel") {
                const std::string name   = params.value("name",   "");
                const std::string sensor = params.value("sensor", "");
                const std::string pwm    = params.value("pwm",    "");
                bool ok = rpcCreateChannel(name, sensor, pwm);
                return result_obj(id, json{{"ok", ok}});
            }
            if (method == "deleteChannel") {
                const std::string cid = params.value("id", "");
                bool ok = rpcDeleteChannel(cid);
                return result_obj(id, json{{"ok", ok}});
            }
            if (method == "setChannelMode") {
                const std::string cid = params.value("id", "");
                const std::string md  = params.value("mode", "Auto");
                bool ok = rpcSetChannelMode(cid, md);
                return result_obj(id, json{{"ok", ok}});
            }
            if (method == "setChannelManual") {
                const std::string cid = params.value("id", "");
                const double pct = params.value("pct", 0.0);
                bool ok = rpcSetChannelManual(cid, pct);
                return result_obj(id, json{{"ok", ok}});
            }
            if (method == "setChannelCurve") {
                const std::string cid = params.value("id", "");
                std::vector<CurvePoint> pts;
                if (params.contains("points") && params["points"].is_array()) {
                    for (auto& p : params["points"]) {
                        if (p.is_array() && p.size()==2) {
                            pts.push_back(CurvePoint{p[0].get<double>(), p[1].get<double>()});
                        } else if (p.is_object() && p.contains("x") && p.contains("y")) {
                            pts.push_back(CurvePoint{p["x"].get<double>(), p["y"].get<double>()});
                        }
                    }
                }
                bool ok = rpcSetChannelCurve(cid, pts);
                return result_obj(id, json{{"ok", ok}});
            }
            if (method == "setChannelHystTau") {
                const std::string cid = params.value("id", "");
                const double hyst = params.value("hyst", 0.0);
                const double tau  = params.value("tau",  0.0);
                bool ok = rpcSetChannelHystTau(cid, hyst, tau);
                return result_obj(id, json{{"ok", ok}});
            }
            if (method == "engineStart") {
                bool ok = engine_.start();
                return result_obj(id, json{{"ok", ok}});
            }
            if (method == "engineStop") {
                engine_.stop();
                return result_obj(id, json{{"ok", true}});
            }
            if (method == "deleteCoupling") {
                const std::string cid = params.value("id", "");
                bool ok = rpcDeleteCoupling(cid);
                return result_obj(id, json{{"ok", ok}});
            }

            return error_obj(id, -32601, "Method not found");
        } catch (const std::exception& e) {
            return error_obj(id, -32603, std::string("Internal error: ")+e.what());
        }
    }

    json rpcEnumerate() {
        // Discover temps & pwms once per call (cheap)
        auto temps = hw_.discoverTemps();
        auto pwms  = hw_.discoverPwms();

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
            // Build a friendly label: "<hwmonX>:<pwmN>"
            std::string label = p.pwm_path;
            {
                auto pos = label.rfind('/');
                if (pos != std::string::npos) {
                    auto base = label.substr(pos+1); // pwmN
                    auto dir  = label.substr(0, pos);
                    auto pos2 = dir.rfind('/');
                    if (pos2 != std::string::npos) dir = dir.substr(pos2+1);
                    label = dir + ":" + base;
                }
            }
            jp.push_back(json{
                {"label",  label},
                {"pwm",    p.pwm_path},
                {"enable", p.enable_path},
                {"tach",   p.tach_path}
            });
        }

        return json{
            {"sensors", jt},
            {"pwms",    jp}
        };
    }

    json rpcListChannels() {
        const auto chs = engine_.snapshot();
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

    bool rpcCreateChannel(const std::string& name,
                          const std::string& sensor,
                          const std::string& pwm) {
        // Simple implementation: append to snapshot and set back
        auto chs = engine_.snapshot();
        Channel c;
        c.id = std::to_string(static_cast<unsigned long long>(
            std::chrono::steady_clock::now().time_since_epoch().count()));
        c.name = name;
        c.sensor_path = sensor;
        c.pwm_path = pwm;
        c.mode = "Auto";
        c.curve = { {20,20}, {40,40}, {60,60}, {80,100} };
        chs.push_back(std::move(c));
        engine_.setChannels(std::move(chs));
        return true;
                          }

                          bool rpcDeleteChannel(const std::string& id) {
                              if (id.empty()) return false;
                              auto chs = engine_.snapshot();
                              const auto n0 = chs.size();
                              chs.erase(std::remove_if(chs.begin(), chs.end(),
                                                       [&](const Channel& c){ return c.id == id; }),
                                        chs.end());
                              if (chs.size() == n0) return false;
                              engine_.setChannels(std::move(chs));
                              return true;
                          }

                          bool rpcSetChannelMode(const std::string& id, const std::string& mode) {
                              engine_.updateChannelMode(id, mode);
                              return true;
                          }

                          bool rpcSetChannelManual(const std::string& id, double pct) {
                              engine_.updateChannelManual(id, pct);
                              return true;
                          }

                          bool rpcSetChannelCurve(const std::string& id, const std::vector<CurvePoint>& pts) {
                              engine_.updateChannelCurve(id, pts);
                              return true;
                          }

                          bool rpcSetChannelHystTau(const std::string& id, double hyst, double tau) {
                              engine_.updateChannelHystTau(id, hyst, tau);
                              return true;
                          }

                          bool rpcDeleteCoupling(const std::string& /*id*/) {
                              // Placeholder: depends on your coupling model; return true for now.
                              return true;
                          }

                          // ---------- JSON-RPC helpers ----------
                          static json error_obj(const json& id, int code, const std::string& msg) {
                              json r = {
                                  {"jsonrpc","2.0"},
                                  {"error", {{"code", code}, {"message", msg}}}
                              };
                              if (!id.is_null()) r["id"] = id;
                              return r;
                          }

                          static json result_obj(const json& id, const json& result) {
                              json r = {
                                  {"jsonrpc","2.0"},
                                  {"result", result}
                              };
                              if (!id.is_null()) r["id"] = id;
                              return r;
                          }

                          // ---------- accept loop ----------
                          void acceptLoop() {
                              std::fprintf(stderr, "[daemon] listening on %s\n", sockPath_.c_str());
                              while (running_) {
                                  int cfd = ::accept4(srv_, nullptr, nullptr, 0);
                                  if (cfd < 0) {
                                      if (errno == EINTR) continue;
                                      if (!running_) break;
                                      std::this_thread::sleep_for(std::chrono::milliseconds(50));
                                      continue;
                                  }
                                  std::thread(&Daemon::serveClient, this, cfd).detach();
                              }
                          }

                          void serveClient(int cfd) {
                              // Read one line => may be single object or array (batch). Return one line.
                              std::string line;
                              if (!read_line(cfd, line)) { ::close(cfd); return; }

                              json reply;
                              try {
                                  json req = json::parse(line);
                                  if (req.is_array()) {
                                      json arr = json::array();
                                      for (auto& one : req) {
                                          json r = dispatch(one);
                                          // Spec: notifications (no id) should not be responded;
                                          // We include only if "id" exists.
                                          if (r.contains("id")) arr.push_back(std::move(r));
                                      }
                                      reply = arr;
                                  } else {
                                      reply = dispatch(req);
                                  }
                              } catch (const std::exception& e) {
                                  reply = error_obj(nullptr, -32700, std::string("Parse error: ")+e.what());
                              }

                              const std::string payload = reply.dump() + "\n";
                              write_all(cfd, payload.data(), payload.size());
                              ::close(cfd);
                          }

private:
    std::string sockPath_;
    int srv_{-1};
    std::atomic<bool> running_{false};
    std::thread th_;

    Hwmon  hw_;
    Engine engine_;
};

// ---------------------------- optional entry ----------------------------
// If your project uses a separate main.cpp that instantiates Daemon, you can
// omit this block. It is guarded by a macro to avoid duplicate main symbols.

#ifdef LFC_DAEMON_STANDALONE
int main() {
    Daemon d;
    if (!d.start()) return 1;
    // Simple wait loop; in real service, integrate with systemd or signals.
    for (;;) std::this_thread::sleep_for(std::chrono::seconds(60));
    return 0;
}
#endif
