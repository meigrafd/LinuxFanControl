/*
 * Linux Fan Control — Daemon
 * Dynamic RPC command registry, engine, detection, channels, logging.
 * (c) 2025 LinuxFanControl contributors
 */

#include "Daemon.hpp"
#include <sys/file.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <cstdarg>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <random>
#include <chrono>
#include <thread>

using namespace std::chrono_literals;
using jsonlite::Value; using jsonlite::Object; using jsonlite::Array;

namespace {

  inline std::string jsonEscape(const std::string& s) {
    // minimal escape for messages
    std::ostringstream o; o << '"';
    for (char c : s) {
      switch (c) { case '\"': o<<"\\\""; break; case '\\': o<<"\\\\"; break; case '\n': o<<"\\n"; break;
        case '\r': o<<"\\r"; break; case '\t': o<<"\\t"; break; default: o<<c; }
    }
    o << '"'; return o.str();
  }

} // anon

Daemon::Daemon() = default;

Daemon::~Daemon() {
  requestShutdown();
  if (engineThr_.joinable()) engineThr_.join();
  closeLogfile();
  removePidfile();
}

bool Daemon::init(const Options& opt, std::string& err) {
  opts_ = opt;
  if (!openLogfile(err)) return false;
  if (!writePidfile(err)) return false;

  rescanHwmon();
  registerBuiltins();

  running_ = true;
  wantStop_ = false;
  logf("[init] pid=%d log=%s debug=%d bind=%s:%u\n",
       getpid(), opts_.logfile.c_str(), (int)opts_.debug,
       opts_.bindHost.c_str(), (unsigned)opts_.bindPort);
  return true;
}

void Daemon::run() {
  while (running_ && !wantStop_) std::this_thread::sleep_for(50ms);
}

void Daemon::pumpOnce(int) {
  std::this_thread::sleep_for(10ms);
}

void Daemon::requestShutdown() {
  wantStop_ = true;
  engineOn_.store(false);
}

bool Daemon::isRunning() const {
  return running_ && !wantStop_;
}

bool Daemon::addCommand(const std::string& name, const std::string& description, Handler h) {
  std::lock_guard<std::mutex> lk(regMx_);
  if (reg_.count(name)) return false;
  reg_.emplace(name, Command{name, description, std::move(h)});
  return true;
}

bool Daemon::hasCommand(const std::string& name) const {
  std::lock_guard<std::mutex> lk(regMx_);
  return reg_.count(name) != 0;
}

std::vector<Daemon::Command> Daemon::listCommands() const {
  std::lock_guard<std::mutex> lk(regMx_);
  std::vector<Command> out;
  out.reserve(reg_.size());
  for (auto& kv : reg_) out.push_back(kv.second);
  std::sort(out.begin(), out.end(), [](auto& a, auto& b){ return a.name < b.name; });
  return out;
}

std::string Daemon::dispatch(const std::string& method, const std::string& params) {
  Handler fn;
  {
    std::lock_guard<std::mutex> lk(regMx_);
    auto it = reg_.find(method);
    if (it == reg_.end()) {
      return std::string("{\"error\":{\"code\":-32601,\"message\":\"Method not found: ")
      + method + "\"}}";
}
fn = it->second.fn;
}
try {
  return fn(params);
} catch (const std::exception& e) {
  return std::string("{\"error\":{\"code\":-32000,\"message\":") + jsonEscape(e.what()) + "}}";
  }
  }

  /* ---------- helpers ---------- */

  void Daemon::rescanHwmon() {
    std::lock_guard<std::mutex> lk(hwMx_);
    hw_ = hwmon::scanSysfs();
    logf("[hwmon] sensors=%zu pwms=%zu\n", hw_.sensors.size(), hw_.pwms.size());
  }

  Daemon::Channel* Daemon::findChannel(const std::string& id) {
    std::lock_guard<std::mutex> lk(chMx_);
    for (auto& c : channels_) if (c.id == id) return &c;
    return nullptr;
  }

  const HwmonPwm* Daemon::findPwm(const std::string& pwmId) const {
    std::lock_guard<std::mutex> lk(hwMx_);
    for (auto& p : hw_.pwms) if (p.id == pwmId) return &p;
    return nullptr;
  }

  double Daemon::evalCurveDuty(const Channel& ch, double tempC) const {
    if (ch.curve.empty()) return ch.manualDuty;
    // piecewise linear
    auto pts = ch.curve;
    std::sort(pts.begin(), pts.end(), [](auto& a, auto& b){ return a.x < b.x; });
    if (tempC <= pts.front().x) return pts.front().y;
    if (tempC >= pts.back().x)  return pts.back().y;
    for (size_t i=1;i<pts.size();++i) {
      if (tempC <= pts[i].x) {
        auto& a = pts[i-1]; auto& b = pts[i];
        double t = (tempC - a.x) / (b.x - a.x);
        return a.y + t * (b.y - a.y);
      }
    }
    return pts.back().y;
  }

  std::optional<HwmonSensor> Daemon::pickBestTempSensor() const {
    std::lock_guard<std::mutex> lk(hwMx_);
    for (auto& s : hw_.sensors) if (s.type=="temp") return s;
    return std::nullopt;
  }

  /* ---------- engine ---------- */

  void Daemon::engineThreadFn() {
    logf("[engine] started\n");
    auto lastUpdate = std::chrono::steady_clock::now();

    while (engineOn_.load() && !wantStop_) {
      // read reference temp
      double refTemp = 40.0;
      if (auto ts = pickBestTempSensor()) {
        if (auto v = hwmon::readValue(*ts)) refTemp = *v;
      }

      // drive channels
      std::vector<Channel> snapshot;
      {
        std::lock_guard<std::mutex> lk(chMx_);
        snapshot = channels_;
      }

      for (auto& ch : snapshot) {
        double duty = ch.mode==Mode::Manual ? ch.manualDuty : evalCurveDuty(ch, refTemp);
        if (duty < 0) duty = 0; if (duty > 100) duty = 100;
        int raw = int((duty/100.0)*255.0 + 0.5);
        auto* pwm = findPwm(ch.pwmId);
        if (!pwm) continue;
        hwmon::enableManual(*pwm);
        hwmon::writePwmRaw(*pwm, raw);
      }

      std::this_thread::sleep_for(250ms);
    }
    logf("[engine] stopped\n");
  }

  /* ---------- JSON helpers ---------- */

  std::string Daemon::result(const Value& v) {
    Object wrap; wrap["result"] = v;
    return jsonlite::stringify(Value(std::move(wrap)));
  }

  std::string Daemon::ok() {
    Object r; r["ok"] = Value(true);
    return result(Value(std::move(r)));
  }

  /* ---------- built-in RPC ---------- */

  void Daemon::registerBuiltins() {
    // Introspection
    addCommand("rpc.list", "List all available RPC commands", [this](const std::string&) {
      Array arr;
      for (auto& c : listCommands()) {
        Object o; o["name"]=Value(c.name); o["description"]=Value(c.description);
        arr.push_back(Value(std::move(o)));
      }
      return result(Value(std::move(arr)));
    });

    // HW scan
    addCommand("enumerate", "Enumerate sensors and PWM outputs (/sys/class/hwmon)", [this](const std::string&) {
      rescanHwmon();
      Array sArr; Array pArr;
      {
        std::lock_guard<std::mutex> lk(hwMx_);
        for (auto& s : hw_.sensors) {
          Object o; o["id"]=Value(s.id); o["label"]=Value(s.label);
          o["type"]=Value(s.type); o["path"]=Value(s.path);
          sArr.push_back(Value(std::move(o)));
        }
        for (auto& p : hw_.pwms) {
          Object o; o["id"]=Value(p.id); o["label"]=Value(p.label);
          o["pathPwm"]=Value(p.pathPwm); o["pathEnable"]=Value(p.pathEnable);
          o["min"]=Value((double)p.minDuty); o["max"]=Value((double)p.maxDuty);
          o["writable"]=Value(p.writable);
          pArr.push_back(Value(std::move(o)));
        }
      }
      Object out; out["sensors"]=Value(std::move(sArr)); out["pwms"]=Value(std::move(pArr));
      return result(Value(std::move(out)));
    });

    // Simple detection/calibration (safe probing)
    addCommand("detectCalibrate", "Detect PWM->fan coupling by safe PWM perturbation", [this](const std::string& params){
      (void)params;
      rescanHwmon();
      int tested = 0, writable = 0;
      {
        std::lock_guard<std::mutex> lk(hwMx_);
        tested = (int)hw_.pwms.size();
      }
      // Try enabling manual & restoring raw
      {
        std::lock_guard<std::mutex> lk(hwMx_);
        for (auto& p : hw_.pwms) {
          auto prev = hwmon::readPwmRaw(p).value_or(128);
          if (hwmon::enableManual(p)) {
            ++writable;
            int boost = 255; // 100%
            hwmon::writePwmRaw(p, boost);
            std::this_thread::sleep_for(std::chrono::seconds(3)); // short dwell (GUI steuert längere Tests)
    hwmon::writePwmRaw(p, prev);
          }
        }
      }
      Object r; r["tested"]=Value((double)tested); r["writable"]=Value((double)writable);
      return result(Value(std::move(r)));
    });

    // Telemetry pull (current temps/fans)
    addCommand("telemetry.pull", "Return current sensor values", [this](const std::string&){
      Array s;
      std::lock_guard<std::mutex> lk(hwMx_);
      for (auto& x : hw_.sensors) {
        auto v = hwmon::readValue(x);
        Object o; o["id"]=Value(x.id); o["label"]=Value(x.label);
        o["type"]=Value(x.type); o["value"]=Value(v ? *v : 0.0);
        s.push_back(Value(std::move(o)));
      }
      return result(Value(std::move(s)));
    });

    // Channels
    addCommand("listChannels", "List configured channels", [this](const std::string&){
      Array arr;
      std::lock_guard<std::mutex> lk(chMx_);
      for (auto& c : channels_) {
        Object o;
        o["id"]=Value(c.id); o["label"]=Value(c.label);
        o["pwmId"]=Value(c.pwmId);
        o["mode"]=Value(c.mode==Mode::Manual?"Manual":"Auto");
        o["manualDuty"]=Value(c.manualDuty);
        Array pts;
        for (auto& p : c.curve) { Object q; q["x"]=Value(p.x); q["y"]=Value(p.y); pts.push_back(Value(std::move(q))); }
        o["curve"]=Value(std::move(pts));
        o["hystTau"]=Value(c.hystTau);
        arr.push_back(Value(std::move(o)));
      }
      return result(Value(std::move(arr)));
    });

    addCommand("createChannel", "Create a channel (label, pwmId)", [this](const std::string& params){
      Value v; std::string err;
      if (!jsonlite::parse(params.empty()?"{}":params, v, err) || !v.isObject())
        return std::string("{\"error\":{\"code\":-32602,\"message\":\"Invalid params\"}}");
        auto* L = jsonlite::objGet(v, "label");
        auto* P = jsonlite::objGet(v, "pwmId");
        if (!L || !P || !L->isStr() || !P->isStr())
          return std::string("{\"error\":{\"code\":-32602,\"message\":\"label and pwmId required\"}}");
        if (!findPwm(P->asStr()))
          return std::string("{\"error\":{\"code\":-32602,\"message\":\"pwmId not found\"}}");

        Channel c;
        c.id = P->asStr(); // simple: use pwmId as channel id (unique)
    c.label = L->asStr();
    c.pwmId = P->asStr();
    c.mode = Mode::Auto;
    c.manualDuty = 30.0;
    c.curve = { {30,30}, {60,70}, {80,100} };
    c.hystTau = 2.0;

    {
      std::lock_guard<std::mutex> lk(chMx_);
      // replace if exists
      bool replaced=false;
      for (auto& e:channels_) if (e.id==c.id){ e=c; replaced=true; break; }
      if (!replaced) channels_.push_back(std::move(c));
    }
    return ok();
    });

    addCommand("deleteChannel", "Delete channel (id)", [this](const std::string& params){
      Value v; std::string err;
      if (!jsonlite::parse(params.empty()?"{}":params, v, err) || !v.isObject())
        return std::string("{\"error\":{\"code\":-32602,\"message\":\"Invalid params\"}}");
        auto* I = jsonlite::objGet(v, "id");
        if (!I || !I->isStr()) return std::string("{\"error\":{\"code\":-32602,\"message\":\"id required\"}}");
        std::lock_guard<std::mutex> lk(chMx_);
        channels_.erase(std::remove_if(channels_.begin(), channels_.end(),
                                       [&](auto& c){ return c.id==I->asStr(); }), channels_.end());
        return ok();
        });

    addCommand("setChannelMode", "Set channel mode (id, mode=Manual|Auto)", [this](const std::string& params){
      Value v; std::string err;
      if (!jsonlite::parse(params.empty()?"{}":params, v, err) || !v.isObject())
        return std::string("{\"error\":{\"code\":-32602,\"message\":\"Invalid params\"}}");
        auto* I = jsonlite::objGet(v, "id");
        auto* M = jsonlite::objGet(v, "mode");
        if (!I||!M||!I->isStr()||!M->isStr()) return std::string("{\"error\":{\"code\":-32602,\"message\":\"id, mode required\"}}");
        auto* ch = findChannel(I->asStr());
        if (!ch) return std::string("{\"error\":{\"code\":-32602,\"message\":\"channel not found\"}}");
        ch->mode = (M->asStr()=="Manual")?Mode::Manual:Mode::Auto;
        return ok();
        });

    addCommand("setChannelManual", "Set manual duty (id, duty)", [this](const std::string& params){
      Value v; std::string err;
      if (!jsonlite::parse(params.empty()?"{}":params, v, err) || !v.isObject())
        return std::string("{\"error\":{\"code\":-32602,\"message\":\"Invalid params\"}}");
        auto* I = jsonlite::objGet(v, "id");
        auto* D = jsonlite::objGet(v, "duty");
        if (!I||!D||!I->isStr()||!D->isNum()) return std::string("{\"error\":{\"code\":-32602,\"message\":\"id, duty required\"}}");
        auto* ch = findChannel(I->asStr());
        if (!ch) return std::string("{\"error\":{\"code\":-32602,\"message\":\"channel not found\"}}");
        ch->manualDuty = std::clamp(D->asNum(), 0.0, 100.0);
        return ok();
        });

    addCommand("setChannelCurve", "Update curve points (id, points:[{x,y}])", [this](const std::string& params){
      Value v; std::string err;
      if (!jsonlite::parse(params.empty()?"{}":params, v, err) || !v.isObject())
        return std::string("{\"error\":{\"code\":-32602,\"message\":\"Invalid params\"}}");
        auto* I = jsonlite::objGet(v, "id");
        auto* P = jsonlite::objGet(v, "points");
        if (!I||!P||!I->isStr()||!P->isArray()) return std::string("{\"error\":{\"code\":-32602,\"message\":\"id, points required\"}}");
        auto* ch = findChannel(I->asStr());
        if (!ch) return std::string("{\"error\":{\"code\":-32602,\"message\":\"channel not found\"}}");
        std::vector<CurvePoint> pts;
        for (auto& e : P->asArray()) {
          if (!e.isObject()) continue;
          auto* X = jsonlite::objGet(e, "x");
          auto* Y = jsonlite::objGet(e, "y");
          if (!X||!Y||!X->isNum()||!Y->isNum()) continue;
          pts.push_back({X->asNum(), Y->asNum()});
        }
        if (!pts.empty()) ch->curve = std::move(pts);
        return ok();
        });

    addCommand("setChannelHystTau", "Update hysteresis/tau (id, tau)", [this](const std::string& params){
      Value v; std::string err;
      if (!jsonlite::parse(params.empty()?"{}":params, v, err) || !v.isObject())
        return std::string("{\"error\":{\"code\":-32602,\"message\":\"Invalid params\"}}");
        auto* I = jsonlite::objGet(v, "id");
        auto* T = jsonlite::objGet(v, "tau");
        if (!I||!T||!I->isStr()||!T->isNum()) return std::string("{\"error\":{\"code\":-32602,\"message\":\"id, tau required\"}}";
        );
        auto* ch = findChannel(I->asStr());
        if (!ch) return std::string("{\"error\":{\"code\":-32602,\"message\":\"channel not found\"}}");
        ch->hystTau = std::max(0.0, T->asNum());
        return ok();
        });

    addCommand("deleteCoupling", "Remove coupling (noop placeholder)", [this](const std::string&){
      return ok();
    });

    // Engine
    addCommand("engineStart", "Start control engine", [this](const std::string&){
      if (engineOn_.exchange(true)) return ok();
      engineThr_ = std::thread([this]{ engineThreadFn(); });
      return ok();
    });

    addCommand("engineStop", "Stop control engine", [this](const std::string&){
      engineOn_.store(false);
      if (engineThr_.joinable()) engineThr_.join();
      return ok();
    });
    }

    /* ---------- pidfile/log ---------- */

    bool Daemon::writePidfile(std::string& err) {
      pidfd_ = ::open(opts_.pidfile.c_str(), O_RDWR | O_CREAT, 0644);
      if (pidfd_ < 0) { err = "open pidfile failed: " + std::string(std::strerror(errno)); return false; }
      if (flock(pidfd_, LOCK_EX | LOCK_NB) != 0) {
        err = "another instance is running (pidfile locked): " + opts_.pidfile;
        ::close(pidfd_); pidfd_ = -1; return false;
      }
      if (ftruncate(pidfd_, 0) != 0) { err = "truncate pidfile failed: " + std::string(std::strerror(errno)); return false; }
      std::string pid = std::to_string(getpid()) + "\n";
      if (write(pidfd_, pid.data(), pid.size()) < 0) { err = "write pidfile failed: " + std::string(std::strerror(errno)); return false; }
      return true;
    }

    void Daemon::removePidfile() {
      if (pidfd_ >= 0) { ::close(pidfd_); pidfd_ = -1; ::unlink(opts_.pidfile.c_str()); }
    }

    bool Daemon::openLogfile(std::string& err) {
      flog_ = std::fopen(opts_.logfile.c_str(), "a");
      if (!flog_) { err = "open logfile failed: " + opts_.logfile + " (" + std::strerror(errno) + ")"; return false; }
      std::setvbuf(flog_, nullptr, _IOLBF, 0);
      return true;
    }

    void Daemon::closeLogfile() {
      if (flog_) { std::fflush(flog_); std::fclose(flog_); flog_ = nullptr; }
    }

    void Daemon::logf(const char* fmt, ...) {
      if (!flog_) return;
      va_list ap; va_start(ap, fmt); std::vfprintf(flog_, fmt, ap); va_end(ap);
    }
