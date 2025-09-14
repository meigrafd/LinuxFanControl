#pragma once
/*
 * Linux Fan Control — Daemon
 * Dynamic RPC command registry, engine, detection, channels, logging.
 * (c) 2025 LinuxFanControl contributors
 */

#include <string>
#include <unordered_map>
#include <functional>
#include <vector>
#include <mutex>
#include <optional>
#include <thread>
#include <atomic>
#include <cstdio>
#include <cstdint>

#include "Hwmon.hpp"
#include "JsonLite.hpp"

class Daemon {
public:
  using Handler = std::function<std::string(const std::string& params)>;

  struct Command {
    std::string name;
    std::string description;
    Handler     fn;
  };

  struct Options {
    std::string pidfile   = "/tmp/lfcd.pid";
    std::string logfile   = "/tmp/daemon_lfc.log";
    bool        debug     = false;
    std::string bindHost  = "127.0.0.1";
    uint16_t    bindPort  = 8765;
  };

  enum class Mode { Manual, Auto };

  struct CurvePoint { double x=30.0; double y=30.0; }; // x=°C, y=% (0..100)

  struct Channel {
    std::string id;           // stable uuid-ish or pwm id
    std::string label;
    std::string pwmId;        // links to HwmonPwm.id
    Mode mode = Mode::Auto;
    double manualDuty = 30.0; // 0..100
    std::vector<CurvePoint> curve; // sorted by x
    double hystTau = 2.0;     // seconds
  };

public:
  Daemon();
  ~Daemon();

  // lifecycle
  bool init(const Options& opt, std::string& err);
  void run();
  void pumpOnce(int timeoutMs);
  void requestShutdown();
  bool isRunning() const;

  // dynamic command API
  bool addCommand(const std::string& name, const std::string& description, Handler h);
  bool hasCommand(const std::string& name) const;
  std::vector<Command> listCommands() const;

  // dispatch entry used by RPC server
  std::string dispatch(const std::string& method, const std::string& params);

  // logging helpers
  void logf(const char* fmt, ...);
  void setDebug(bool on) { opts_.debug = on; }
  const Options& options() const { return opts_; }

private:
  // built-ins
  void registerBuiltins();

  // sysfs
  void rescanHwmon();

  // engine
  void engineThreadFn();
  double evalCurveDuty(const Channel& ch, double tempC) const;
  std::optional<HwmonSensor> pickBestTempSensor() const; // naive: first temp sensor

  // channel helpers
  Channel* findChannel(const std::string& id);
  const HwmonPwm* findPwm(const std::string& pwmId) const;

  // pidfile/log
  bool writePidfile(std::string& err);
  void removePidfile();
  bool openLogfile(std::string& err);
  void closeLogfile();

  // JSON helpers
  static std::string result(const jsonlite::Value& v);
  static std::string ok();

private:
  Options opts_;

  HwmonSnapshot hw_;
  std::mutex hwMx_;

  std::vector<Channel> channels_;
  std::mutex chMx_;

  std::unordered_map<std::string, Command> reg_;
  mutable std::mutex regMx_;

  std::thread engineThr_;
  std::atomic<bool> engineOn_{false};

  volatile bool running_ = false;
  volatile bool wantStop_ = false;

  FILE* flog_ = nullptr;
  int   pidfd_ = -1;
};
