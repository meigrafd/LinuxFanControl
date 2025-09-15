#pragma once
/*
 * Linux Fan Control — Daemon
 * JSON-RPC over TCP (no HTTP), SHM telemetry, detection, channels, logging.
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
#include "Config.hpp"
#include "ShmTelemetry.hpp"

class RpcTcpServer;

class Daemon {
public:
  using Handler = std::function<std::string(const std::string& params)>;

  struct Command {
    std::string name;
    std::string description;
    Handler     fn;
  };

  enum class Mode { Manual, Auto };

  struct CurvePoint { double x=30.0; double y=30.0; }; // x=°C, y=% (0..100)

  struct Channel {
    std::string id;           // stable id (here: pwm id)
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
  bool init(const DaemonConfig& cfg, std::string& err);
  void run();
  void pumpOnce(int timeoutMs);
  void requestShutdown();
  bool isRunning() const;

  // dynamic command API
  bool addCommand(const std::string& name, const std::string& description, Handler h);
  std::vector<Command> listCommands() const;

  // dispatch entry used by RPC server
  std::string dispatch(const std::string& method, const std::string& params);

  // logging helpers
  void logf(const char* fmt, ...);
  void setDebug(bool on) { config_.debug = on; }
  const DaemonConfig& config() const { return config_; }

private:
  // built-ins
  void registerBuiltins();

  // sysfs
  void rescanHwmon();

  // detection (aggressive)
  jsonlite::Value detectAggressive();

  // engine
  void engineThreadFn();
  double evalCurveDuty(const Channel& ch, double tempC) const;
  std::optional<HwmonSensor> pickBestTempSensor() const;

  // channel helpers
  Channel* findChannel(const std::string& id);
  const HwmonPwm* findPwm(const std::string& pwmId) const;

  // pidfile/log
  bool writePidfile(std::string& err);
  void removePidfile();
  bool openLogfile(std::string& err);
  void closeLogfile();
  void rotateLogIfNeeded(size_t addBytes);

  // JSON helpers
  static std::string result(const jsonlite::Value& v);
  static std::string ok();

  // profile persistence
  bool loadActiveProfile(std::string& err);
  bool saveActiveProfile(std::string& err);

private:
  DaemonConfig config_;

  HwmonSnapshot hw_;
  mutable std::mutex hwMx_;

  std::vector<Channel> channels_;
  mutable std::mutex chMx_;

  std::unordered_map<std::string, Command> reg_;
  mutable std::mutex regMx_;

  std::thread engineThr_;
  std::atomic<bool> engineOn_{false};

  volatile bool running_ = false;
  volatile bool wantStop_ = false;

  FILE* flog_ = nullptr;
  int   pidfd_ = -1;
  size_t logBytes_ = 0;

  ShmPublisher shm_;
  friend class RpcTcpServer;
};
