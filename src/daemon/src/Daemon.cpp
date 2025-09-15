/*
 * Linux Fan Control — Daemon
 * JSON-RPC over TCP, SHM telemetry, detection, channels, logging.
 * (c) 2025 LinuxFanControl contributors
 */
#include "Daemon.hpp"
#include "RpcTcpServer.hpp"
#include "Log.hpp"
#include "include/CommandIntrospection.h"
#include "Hwmon.hpp"

#include <fstream>
#include <thread>
#include <sstream>

#include <sys/stat.h>   // umask, mkdir
#include <unistd.h>     // getpid, unlink

using namespace lfc;

Daemon::Daemon() = default;
Daemon::~Daemon() { shutdown(); }

bool Daemon::writePidFile(const std::string& path) {
  pidFile_ = path;
  ::umask(0022);
  auto pos = path.rfind('/');
  if (pos!=std::string::npos) {
    std::string dir=path.substr(0,pos);
    ::mkdir(dir.c_str(),0755);
  }
  std::ofstream f(path, std::ios::trunc);
  if (!f) return false;
  f<<getpid()<<"\n";
  return true;
}
void Daemon::removePidFile() { if (!pidFile_.empty()) ::unlink(pidFile_.c_str()); }

// -----------------------------
// Init mit neuem Config-Layout
// -----------------------------
bool Daemon::init(const DaemonConfig& cfg, bool debugCli) {
  // Logger
  Logger::instance().configure(cfg.log.file, cfg.log.maxBytes, cfg.log.rotateCount, cfg.log.debug || debugCli);
  if (cfg.log.debug || debugCli) Logger::instance().setLevel(LogLevel::Debug);
  LFC_LOGI("daemon: init");

  // PID-File (best effort)
  if (!writePidFile(cfg.pidFile)) LFC_LOGW("pidfile create failed: %s", cfg.pidFile.c_str());

  // hwmon inventory
  hwmon_ = Hwmon::scan();
  LFC_LOGI("hwmon: found %zu pwms, %zu fans, %zu temps", hwmon_.pwms.size(), hwmon_.fans.size(), hwmon_.temps.size());

  // Engine + SHM
  engine_.setSnapshot(hwmon_);
  engine_.initShm(cfg.shm.path);

  // RPC-Registry
  reg_.reset(new CommandRegistry());
  bindCommands(*reg_);

  // TCP RPC-Server
  rpc_ = std::make_unique<RpcTcpServer>(*this, cfg.rpc.host, static_cast<std::uint16_t>(cfg.rpc.port), /*debug*/(cfg.log.debug || debugCli));
  std::string err;
  if (!rpc_->start(err)) {
    LFC_LOGE("rpc start failed: %s", err.c_str());
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
  LFC_LOGI("daemon: stopped");
}

void Daemon::runLoop() {
  engine_.start();
  while (running_) pumpOnce();
}

void Daemon::pumpOnce(int timeoutMs) {
  if (rpc_) rpc_->pumpOnce(timeoutMs);
  engine_.tick();
}

std::string Daemon::jsonError(const char* msg) const {
  std::string s="{\"code\":-32000,\"message\":\"";
  for (const char* p=msg; *p; ++p){ if(*p=='"'||*p=='\\') s.push_back('\\'); s.push_back(*p); }
  s+="\"}";
  return s;
}

// -----------------------------
// Dispatch + Introspection (für RpcTcpServer / CLI)
// -----------------------------
std::string Daemon::dispatch(const std::string& method, const std::string& paramsJson) {
  if (!reg_) return "{\"error\":{\"code\":-32601,\"message\":\"no registry\"}}";
  RpcRequest req{method, /*id*/"", paramsJson};
  auto it = reg_->handlers().find(method);
  if (it == reg_->handlers().end()) return "{\"error\":{\"code\":-32601,\"message\":\"method not found\"}}";
  RpcResult r = it->second(req);
  // Falls Handler bereits vollst. Objekt liefert (result/error Felder), direkt durchreichen
  return r.json;
  }

  std::vector<CommandInfo> Daemon::listRpcCommands() const {
    if (!reg_) return {};
    std::vector<CommandInfo> out;
    out.reserve(reg_->handlers().size());
    for (auto& kv : reg_->handlers()) {
      CommandInfo ci;
      ci.name = kv.first;
      // Help-Text aus Registry, falls vorhanden
      auto it = reg_->help().find(kv.first);
      ci.help = (it==reg_->help().end()? std::string() : it->second);
      out.push_back(std::move(ci));
    }
    return out;
  }
