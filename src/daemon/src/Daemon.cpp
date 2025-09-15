/*
 * Linux Fan Control — Daemon (implementation)
 * - JSON-RPC (TCP) server lifecycle
 * - SHM telemetry + engine bootstrap
 * - PID file handling and logging setup
 * - Command registry wiring
 * (c) 2025 LinuxFanControl contributors
 */
#include "Daemon.hpp"
#include "RpcTcpServer.hpp"
#include "Hwmon.hpp"
#include "Config.hpp"
#include "include/CommandRegistry.h"
#include "Log.hpp"

#include <fstream>
#include <sstream>
#include <thread>
#include <filesystem>
#include <unistd.h>
#include <sys/stat.h>

namespace lfc {

Daemon::Daemon() = default;
Daemon::~Daemon() { shutdown(); }

bool Daemon::writePidFile(const std::string& path) {
  pidFile_ = path;
  auto dir = std::filesystem::path(path).parent_path();
  if (!dir.empty()) {
    std::error_code ec; std::filesystem::create_directories(dir, ec);
  }
  std::ofstream f(path, std::ios::trunc);
  if (!f) return false;
  f << getpid() << "\n";
  return true;
}

void Daemon::removePidFile() {
  if (!pidFile_.empty()) {
    std::error_code ec; std::filesystem::remove(pidFile_, ec);
  }
}

bool Daemon::init(const DaemonConfig& cfg, bool debugCli) {
  Logger::instance().configure(cfg.log.file, cfg.log.maxBytes, cfg.log.rotateCount, cfg.log.debug || debugCli);
  if (cfg.log.debug || debugCli) Logger::instance().setLevel(LogLevel::Debug);

  if (!writePidFile(cfg.pidFile)) {
    LFC_LOGW("pidfile create failed: %s", cfg.pidFile.c_str());
  }

  hwmon_ = Hwmon::scan();
  engine_.setSnapshot(hwmon_);
  engine_.initShm(cfg.shm.path);

  reg_ = std::make_unique<CommandRegistry>();
  bindCommands(*reg_);

  rpc_ = std::make_unique<RpcTcpServer>(*this, cfg.rpc.host, static_cast<std::uint16_t>(cfg.rpc.port), (cfg.log.debug || debugCli));
  if (!rpc_->start(reg_.get())) {
    LFC_LOGE("rpc start failed");
    return false;
  }

  running_ = true;
  return true;
}

void Daemon::shutdown() {
  if (!running_) return;
  running_ = false;
  if (rpc_) { rpc_->stop(); rpc_.reset(); }
  engine_.stop();
  removePidFile();
}

void Daemon::runLoop() {
  engine_.start();
  while (running_) pumpOnce();
}

void Daemon::pumpOnce(int /*timeoutMs*/) { engine_.tick(); }

static std::string jsonBool(bool b) { return b ? "true" : "false"; }
static std::string jsonStr(const std::string& s) {
  std::string out; out.reserve(s.size()+2); out.push_back('"');
  for (char c : s) { if (c=='"'||c=='\\') out.push_back('\\'); out.push_back(c); }
  out.push_back('"'); return out;
}

void Daemon::bindCommands(CommandRegistry& reg) {
  reg.add("lfc.ping", "Health check", [](const RpcRequest&) -> RpcResult {
    return {true, "{\"ok\":true}"};
  });

  reg.add("lfc.cmds", "List available commands", [this](const RpcRequest&) -> RpcResult {
    std::ostringstream ss;
    ss << "{\"commands\":[";
    auto list = this->listRpcCommands();
    for (size_t i=0;i<list.size();++i) {
      if (i) ss << ",";
      ss << "{\"name\":" << jsonStr(list[i].name)
      << ",\"help\":" << jsonStr(list[i].help) << "}";
    }
    ss << "]}";
    return {true, ss.str()};
  });

  reg.add("engine.enable", "Enable/disable automatic control; params: {\"on\":bool}", [this](const RpcRequest& req) -> RpcResult {
    bool on = (req.paramsJson.find("\"on\":true") != std::string::npos);
    this->engine_.enableControl(on);
    return {true, std::string("{\"control\":") + jsonBool(on) + "}"};
  });

  reg.add("engine.status", "Basic engine status", [this](const RpcRequest&) -> RpcResult {
    std::ostringstream ss;
    ss << "{\"control\":" << jsonBool(this->engine_.controlEnabled())
    << ",\"pwms\":" << this->hwmon_.pwms.size()
    << ",\"fans\":" << this->hwmon_.fans.size()
    << ",\"temps\":" << this->hwmon_.temps.size()
    << "}";
    return {true, ss.str()};
  });

  reg.add("hwmon.snapshot", "Counts of discovered devices", [this](const RpcRequest&) -> RpcResult {
    std::ostringstream ss;
    ss << "{\"pwms\":" << this->hwmon_.pwms.size()
    << ",\"fans\":" << this->hwmon_.fans.size()
    << ",\"temps\":" << this->hwmon_.temps.size() << "}";
    return {true, ss.str()};
  });

  reg.add("telemetry.json", "Return current SHM JSON blob", [this](const RpcRequest&) -> RpcResult {
    std::string s;
    if (!this->engine_.getTelemetry(s)) return {false, "{\"error\":{\"code\":-32001,\"message\":\"no telemetry\"}}"};
    std::ostringstream ss;
    ss << "{\"telemetry\":" << (s.empty() ? "null" : s) << "}";
    return {true, ss.str()};
  });
}

std::string Daemon::dispatch(const std::string& method, const std::string& paramsJson) {
  if (!reg_) return "{\"error\":{\"code\":-32601,\"message\":\"no registry\"}}";
  RpcRequest req{method, "", paramsJson};
  auto res = reg_->call(req);
  return res.json;
}

std::vector<CommandInfo> Daemon::listRpcCommands() const {
  if (!reg_) return {};
  return reg_->list();
}

} // namespace lfc
