/*
 * Linux Fan Control — Daemon
 * JSON-RPC over TCP, SHM telemetry, detection, channels, logging.
 * (c) 2025 LinuxFanControl contributors
 */

#include "Daemon.hpp"
#include "RpcTcpServer.hpp"
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
#include <filesystem>

using namespace std::chrono_literals;
using jsonlite::Value; using jsonlite::Object; using jsonlite::Array;
namespace fs = std::filesystem;

namespace {

  inline std::string jsonEscape(const std::string& s) {
    std::ostringstream o; o << '"';
    for (char c : s) {
      switch (c) { case '\"': o<<"\\\""; break; case '\\': o<<"\\\\"; break; case '\n': o<<"\\n"; break;
        case '\r': o<<"\\r"; break; case '\t': o<<"\\t"; break; default: o<<c; }
    }
    o << '"'; return o.str();
  }

  static std::string dirnameOf(const std::string& p) {
    auto pos = p.find_last_of('/');
    return (pos==std::string::npos) ? "." : p.substr(0,pos);
  }

} // anon

Daemon::Daemon() = default;

Daemon::~Daemon() {
  requestShutdown();
  if (engineThr_.joinable()) engineThr_.join();
  closeLogfile();
  removePidfile();
}

bool Daemon::init(const DaemonConfig& cfgIn, std::string& err) {
  config_ = cfgIn;

  // dirs & default profile file(s)
  if (!cfg::ensureDirs(config_, err)) return false;

  if (!openLogfile(err)) return false;
  if (!writePidfile(err)) return false;

  // SHM telemetry ring: 1024 bytes * 512 slots
  if (!shm_.openOrCreate(config_.shm_path, 1024, 512, err)) return false;

  rescanHwmon();
  registerBuiltins();

  // load active profile (channels)
  if (!loadActiveProfile(err)) logf("[warn] loadActiveProfile: %s\n", err.c_str());

  running_ = true;
  wantStop_ = false;
  logf("[init] pid=%d log=%s debug=%d rpc=%s:%u shm=%s\n",
       getpid(), config_.logfile.c_str(), (int)config_.debug,
       config_.rpc_host.c_str(), (unsigned)config_.rpc_port,
       config_.shm_path.c_str());
  return true;
}

void Daemon::run() {
  // RPC server
  std::string err;
  RpcTcpServer rpc(*this, config_.rpc_host, config_.rpc_port, config_.debug);
  if (!rpc.start(err)) {
    logf("[fatal] rpc start failed: %s\n", err.c_str());
    return;
  }

  // engine thread starts on demand; publish telemetry idle
  while (running_ && !wantStop_) {
    // periodic telemetry
    Array arr;
    {
      std::lock_guard<std::mutex> lk(hwMx_);
      for (auto& x : hw_.sensors) {
        auto v = hwmon::readValue(x);
        Object o; o["id"]=Value(x.id); o["type"]=Value(x.type); o["label"]=Value(x.label);
        o["value"]=Value(v ? *v : 0.0);
        arr.push_back(Value(std::move(o)));
      }
    }
    Object t; t["ts"]=Value((double)std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::system_clock::now().time_since_epoch()).count()/1000.0);
    t["sensors"]=Value(std::move(arr));
    shm_.publish(jsonlite::stringify(Value(std::move(t))));
    std::this_thread::sleep_for(1000ms);
  }
  rpc.stop();
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
    while (engineOn_.load() && !wantStop_) {
      double refTemp = 40.0;
      if (auto ts = pickBestTempSensor()) {
        if (auto v = hwmon::readValue(*ts)) refTemp = *v;
      }

      std::vector<Channel> snapshot;
      {
        std::lock_guard<std::mutex> lk(chMx_);
        snapshot = channels_;
      }

      for (auto& ch : snapshot) {
        double duty = ch.mode==Mode::Manual ? ch.manualDuty : evalCurveDuty(ch, refTemp);
        duty = std::clamp(duty, 0.0, 100.0);
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

  /* ---------- detection (aggressive) ---------- */

  jsonlite::Value Daemon::detectAggressive() {
    // Strategy:
    //  - For each PWM: enable manual, capture baseline fan RPMs
    //  - Boost to 100% for 10s
    //  - Measure deltas per fan; any delta > max(3% of baseline, 50 RPM) is "coupled"
    //  - Restore previous PWM raw if readable
    Array out;
    std::lock_guard<std::mutex> lkh(hwMx_);
    for (auto& p : hw_.pwms) {
      Object res; res["pwmId"] = Value(p.id);
      bool manual = hwmon::enableManual(p);
      auto prev = hwmon::readPwmRaw(p).value_or(128);

      // baseline
      std::vector<std::pair<std::string,double>> base;
      for (auto& s : hw_.sensors) if (s.type=="fan") {
        auto v = hwmon::readValue(s).value_or(0.0);
        base.emplace_back(s.id, v);
      }

      // boost
      hwmon::writePwmRaw(p, 255);
      auto t0 = std::chrono::steady_clock::now();
      while (std::chrono::steady_clock::now() - t0 < 10s) std::this_thread::sleep_for(200ms);

      // measure
      Object info;
      Array coupled;
      for (auto& [sid, b] : base) {
        // current
        auto it = std::find_if(hw_.sensors.begin(), hw_.sensors.end(),
                               [&](auto& z){ return z.id==sid; });
        double cur = 0.0;
        if (it != hw_.sensors.end()) cur = hwmon::readValue(*it).value_or(0.0);
        double thr = std::max(b*0.03, 50.0); // 3% or 50 RPM
        if ((cur - b) >= thr) {
          Object c; c["fanId"]=Value(sid); c["rpmBase"]=Value(b); c["rpmNow"]=Value(cur);
          coupled.push_back(Value(std::move(c)));
        }
      }
      info["writable"]=Value(manual);
      info["coupled"]=Value(std::move(coupled));
      res["info"]=Value(std::move(info));

      // restore
      hwmon::writePwmRaw(p, prev);

      out.push_back(Value(std::move(res)));
    }
    return Value(std::move(out));
  }

  /* ---------- built-in RPC ---------- */

  void Daemon::registerBuiltins() {
    addCommand("rpc.version", "Return daemon version & capabilities", [this](const std::string&){
      Object v;
      v["name"]=Value("lfcd");
      v["version"]=Value("0.2.0");
      v["caps"]=Value(Array{ Value("jsonrpc-tcp"), Value("shm-telemetry"), Value("batch") });
      return result(Value(std::move(v)));
    });

    addCommand("rpc.list", "List all available RPC commands", [this](const std::string&) {
      Array arr;
      for (auto& c : listCommands()) {
        Object o; o["name"]=Value(c.name); o["description"]=Value(c.description);
        arr.push_back(Value(std::move(o)));
      }
      return result(Value(std::move(arr)));
    });

    addCommand("shm.info", "Return SHM ring info", [this](const std::string&){
      Object o; o["path"]=Value(config_.shm_path);
      o["slotSize"]=Value((double)shm_.slotSize());
      o["capacity"]=Value((double)shm_.capacity());
      return result(Value(std::move(o)));
    });

    addCommand("enumerate", "Enumerate sensors and PWM outputs", [this](const std::string&) {
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

    addCommand("detectCalibrate", "Aggressive coupling detection (100%% for 10s)", [this](const std::string&){
      auto v = detectAggressive();
      return result(v);
    });

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
        c.id = P->asStr();
        c.label = L->asStr();
        c.pwmId = P->asStr();
        c.mode = Mode::Auto;
        c.manualDuty = 30.0;
        c.curve = { {30,30}, {60,70}, {80,100} };
        c.hystTau = 2.0;

        {
          std::lock_guard<std::mutex> lk(chMx_);
          bool replaced=false;
          for (auto& e:channels_) if (e.id==c.id){ e=c; replaced=true; break; }
          if (!replaced) channels_.push_back(std::move(c));
        }
        std::string serr;
        (void)saveActiveProfile(serr);
        return ok();
        });

    addCommand("deleteChannel", "Delete channel (id)", [this](const std::string& params){
      Value v; std::string err;
      if (!jsonlite::parse(params.empty()?"{}":params, v, err) || !v.isObject())
        return std::string("{\"error\":{\"code\":-32602,\"message\":\"Invalid params\"}}");
        auto* I = jsonlite::objGet(v, "id");
        if (!I || !I->isStr()) return std::string("{\"error\":{\"code\":-32602,\"message\":\"id required\"}}");
        {
          std::lock_guard<std::mutex> lk(chMx_);
          channels_.erase(std::remove_if(channels_.begin(), channels_.end(),
                                         [&](auto& c){ return c.id==I->asStr(); }), channels_.end());
        }
        std::string serr;
        (void)saveActiveProfile(serr);
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
        std::string serr; (void)saveActiveProfile(serr);
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
        std::string serr; (void)saveActiveProfile(serr);
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
        std::string serr; (void)saveActiveProfile(serr);
        return ok();
        });

    addCommand("setChannelHystTau", "Update hysteresis/tau (id, tau)", [this](const std::string& params){
      Value v; std::string err;
      if (!jsonlite::parse(params.empty()?"{}":params, v, err) || !v.isObject())
        return std::string("{\"error\":{\"code\":-32602,\"message\":\"Invalid params\"}}");
        auto* I = jsonlite::objGet(v, "id");
        auto* T = jsonlite::objGet(v, "tau");
        if (!I||!T||!I->isStr()||!T->isNum()) return std::string("{\"error\":{\"code\":-32602,\"message\":\"id, tau required\"}}");
        auto* ch = findChannel(I->asStr());
        if (!ch) return std::string("{\"error\":{\"code\":-32602,\"message\":\"channel not found\"}}");
        ch->hystTau = std::max(0.0, T->asNum());
        std::string serr; (void)saveActiveProfile(serr);
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

    // Config (get/set + save)
    addCommand("config.get", "Return current daemon config", [this](const std::string&){
      Object o;
      o["logfile"]=Value(config_.logfile);
      o["pidfile"]=Value(config_.pidfile);
      o["log_size_bytes"]=Value((double)config_.log_size_bytes);
      o["log_rotate"]=Value((double)config_.log_rotate);
      o["debug"]=Value(config_.debug);
      o["profiles_dir"]=Value(config_.profiles_dir);
      o["active_profile"]=Value(config_.active_profile);
      o["profiles_backup"]=Value(config_.profiles_backup);
      o["rpc_host"]=Value(config_.rpc_host);
      o["rpc_port"]=Value((double)config_.rpc_port);
      o["shm_path"]=Value(config_.shm_path);
      return result(Value(std::move(o)));
    });

    addCommand("config.set", "Set config fields (partial) and persist", [this](const std::string& params){
      Value v; std::string err;
      if (!jsonlite::parse(params.empty()?"{}":params, v, err) || !v.isObject())
        return std::string("{\"error\":{\"code\":-32602,\"message\":\"Invalid params\"}}");

        auto S=[&](const char* k, std::string& dst){ auto* x=jsonlite::objGet(v,k); if(x&&x->isStr()) dst=x->asStr(); };
        auto Z=[&](const char* k, std::size_t& dst){ auto* x=jsonlite::objGet(v,k); if(x&&x->isNum()) dst=(std::size_t)x->asNum(); };
        auto I=[&](const char* k, uint16_t& dst){ auto* x=jsonlite::objGet(v,k); if(x&&x->isNum()) dst=(uint16_t)x->asNum(); };
        auto B=[&](const char* k, bool& dst){ auto* x=jsonlite::objGet(v,k); if(x&&x->isBool()) dst=x->asBool(); };

        S("logfile", config_.logfile);
        S("pidfile", config_.pidfile);
        Z("log_size_bytes", config_.log_size_bytes);
        { std::size_t tmp=config_.log_rotate; Z("log_rotate", tmp); config_.log_rotate=(int)tmp; }
        B("debug", config_.debug);
        S("profiles_dir", config_.profiles_dir);
        S("active_profile", config_.active_profile);
        B("profiles_backup", config_.profiles_backup);
        S("rpc_host", config_.rpc_host);
        I("rpc_port", config_.rpc_port);
        S("shm_path", config_.shm_path);

        std::string e2;
        if (!cfg::ensureDirs(config_, e2)) logf("[warn] ensureDirs: %s\n", e2.c_str());
        if (!saveActiveProfile(e2)) logf("[warn] saveActiveProfile: %s\n", e2.c_str());
        if (!cfg::save(config_, e2)) logf("[warn] config.save: %s\n", e2.c_str());
        return ok();
        });

    // Profiles
    addCommand("profiles.list", "List available profiles", [this](const std::string&){
      Array a;
      for (auto& p : fs::directory_iterator(config_.profiles_dir)) {
        if (p.is_regular_file() && p.path().extension()==".json") {
          a.push_back(Value(p.path().stem().string()));
        }
      }
      Object o; o["active"]=Value(config_.active_profile); o["profiles"]=Value(std::move(a));
      return result(Value(std::move(o)));
    });

    addCommand("profiles.setActive", "Set active profile and load", [this](const std::string& params){
      Value v; std::string err;
      if (!jsonlite::parse(params.empty()?"{}":params, v, err) || !v.isObject())
        return std::string("{\"error\":{\"code\":-32602,\"message\":\"Invalid params\"}}");
        auto* N = jsonlite::objGet(v, "name");
        if (!N || !N->isStr()) return std::string("{\"error\":{\"code\":-32602,\"message\":\"name required\"}}");
        config_.active_profile = N->asStr();
        std::string e2;
        if (!loadActiveProfile(e2)) return std::string("{\"error\":{\"code\":-32000,\"message\":\"load profile failed\"}}");
        (void)cfg::save(config_, e2);
        return ok();
        });
    }

    /* ---------- profile persistence ---------- */

    bool Daemon::loadActiveProfile(std::string& err) {
      auto file = fs::path(config_.profiles_dir) / (config_.active_profile + ".json");
      if (!fs::exists(file)) { channels_.clear(); return true; }
      std::ifstream f(file); if (!f){ err="open profile failed"; return false; }
      std::ostringstream o; o << f.rdbuf();
      jsonlite::Value v; std::string perr;
      if (!jsonlite::parse(o.str(), v, perr) || !v.isObject()) { err="parse profile failed"; return false; }
      auto* A = jsonlite::objGet(v, "channels");
      std::vector<Channel> list;
      if (A && A->isArray()) {
        for (auto& e : A->asArray()) if (e.isObject()) {
          Channel c;
          auto* id = jsonlite::objGet(e,"id");
          auto* lb = jsonlite::objGet(e,"label");
          auto* pw = jsonlite::objGet(e,"pwmId");
          auto* md = jsonlite::objGet(e,"mode");
          auto* du = jsonlite::objGet(e,"manualDuty");
          auto* pt = jsonlite::objGet(e,"curve");
          auto* hy = jsonlite::objGet(e,"hystTau");
          if (id&&id->isStr()) c.id=id->asStr(); else continue;
          if (lb&&lb->isStr()) c.label=lb->asStr();
          if (pw&&pw->isStr()) c.pwmId=pw->asStr();
          if (md&&md->isStr()) c.mode = (md->asStr()=="Manual")?Mode::Manual:Mode::Auto;
          if (du&&du->isNum()) c.manualDuty=du->asNum();
          if (hy&&hy->isNum()) c.hystTau=hy->asNum();
          if (pt&&pt->isArray()) for (auto& q:pt->asArray()) if (q.isObject()) {
            auto* X=jsonlite::objGet(q,"x"); auto* Y=jsonlite::objGet(q,"y");
            if (X&&Y&&X->isNum()&&Y->isNum()) c.curve.push_back({X->asNum(),Y->asNum()});
          }
          list.push_back(std::move(c));
        }
      }
      {
        std::lock_guard<std::mutex> lk(chMx_);
        channels_ = std::move(list);
      }
      return true;
    }

    bool Daemon::saveActiveProfile(std::string& err) {
      auto dir = fs::path(config_.profiles_dir);
      auto file = dir / (config_.active_profile + ".json");
      if (!fs::exists(dir)) { std::error_code ec; fs::create_directories(dir, ec); }
      // backup
      if (config_.profiles_backup && fs::exists(file)) {
        auto bak = dir / (config_.active_profile + ".bak.json");
        std::error_code ec; fs::copy_file(file, bak, fs::copy_options::overwrite_existing, ec);
      }
      // write
      jsonlite::Array arr;
      {
        std::lock_guard<std::mutex> lk(chMx_);
        for (auto& c : channels_) {
          jsonlite::Object o;
          o["id"]=jsonlite::Value(c.id);
          o["label"]=jsonlite::Value(c.label);
          o["pwmId"]=jsonlite::Value(c.pwmId);
          o["mode"]=jsonlite::Value(c.mode==Mode::Manual?"Manual":"Auto");
          o["manualDuty"]=jsonlite::Value(c.manualDuty);
          o["hystTau"]=jsonlite::Value(c.hystTau);
          jsonlite::Array pts;
          for (auto& p : c.curve) { jsonlite::Object q; q["x"]=jsonlite::Value(p.x); q["y"]=jsonlite::Value(p.y); pts.push_back(jsonlite::Value(std::move(q))); }
          o["curve"]=jsonlite::Value(std::move(pts));
          arr.push_back(jsonlite::Value(std::move(o)));
        }
      }
      jsonlite::Object root; root["channels"]=jsonlite::Value(std::move(arr));
      std::ofstream f(file); if (!f){ err="open profile for write failed"; return false; }
      f << jsonlite::stringify(jsonlite::Value(std::move(root)));
      return (bool)f;
    }

    /* ---------- pidfile/log ---------- */

    bool Daemon::writePidfile(std::string& err) {
      pidfd_ = ::open(config_.pidfile.c_str(), O_RDWR | O_CREAT, 0644);
      if (pidfd_ < 0) { err = "open pidfile failed"; return false; }
      if (flock(pidfd_, LOCK_EX | LOCK_NB) != 0) {
        err = "another instance is running (pidfile locked): " + config_.pidfile;
        ::close(pidfd_); pidfd_ = -1; return false;
      }
      if (ftruncate(pidfd_, 0) != 0) { err = "truncate pidfile failed"; return false; }
      std::string pid = std::to_string(getpid()) + "\n";
      if (write(pidfd_, pid.data(), pid.size()) < 0) { err = "write pidfile failed"; return false; }
      return true;
    }

    void Daemon::removePidfile() {
      if (pidfd_ >= 0) { ::close(pidfd_); pidfd_ = -1; ::unlink(config_.pidfile.c_str()); }
    }

    void Daemon::rotateLogIfNeeded(size_t addBytes) {
      if (!flog_) return;
      logBytes_ += addBytes;
      long pos = ftell(flog_);
      if (pos >= 0) logBytes_ = (size_t)pos;

      if (logBytes_ < config_.log_size_bytes) return;

      std::fflush(flog_);
      std::fclose(flog_);
      flog_ = nullptr;

      // rotate: .N ... .1
      for (int i = config_.log_rotate-1; i >= 1; --i) {
        std::string from = config_.logfile + "." + std::to_string(i);
        std::string to   = config_.logfile + "." + std::to_string(i+1);
        std::rename(from.c_str(), to.c_str());
      }
      std::string to1 = config_.logfile + ".1";
      std::rename(config_.logfile.c_str(), to1.c_str());

      std::string err;
      (void)openLogfile(err);
      logBytes_ = 0;
    }

    bool Daemon::openLogfile(std::string& err) {
      std::error_code ec;
      fs::create_directories(fs::path(config_.logfile).parent_path(), ec);
      flog_ = std::fopen(config_.logfile.c_str(), "a");
      if (!flog_) { err = "open logfile failed"; return false; }
      std::setvbuf(flog_, nullptr, _IOLBF, 0);
      return true;
    }

    void Daemon::closeLogfile() {
      if (flog_) { std::fflush(flog_); std::fclose(flog_); flog_ = nullptr; }
    }

    void Daemon::logf(const char* fmt, ...) {
      if (!flog_) return;
      char buf[2048];
      va_list ap; va_start(ap, fmt);
      int n = std::vsnprintf(buf, sizeof(buf), fmt, ap);
      va_end(ap);
      if (n > 0) {
        std::fwrite(buf, 1, (size_t)n, flog_);
        rotateLogIfNeeded((size_t)n);
      }
    }
