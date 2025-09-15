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

bool Daemon::init(const Config& cfg) {
  Logger::instance().configure(cfg.logFile, cfg.logMaxBytes, cfg.logMaxFiles, cfg.debug);
  if (cfg.debug) Logger::instance().setLevel(LogLevel::Debug);
  LFC_LOGI("daemon: init");

  if (!writePidFile(cfg.pidFile)) LFC_LOGW("pidfile create failed: %s", cfg.pidFile.c_str());

  // scan hwmon
  hwmon_ = Hwmon::scan();
  LFC_LOGI("hwmon: found %zu pwms, %zu fans, %zu temps", hwmon_.pwms.size(), hwmon_.fans.size(), hwmon_.temps.size());

  // engine
  engine_.setSnapshot(hwmon_);
  engine_.initShm(cfg.shmPath);

  // rpc
  reg_.reset(new CommandRegistry());
  bindCommands(*reg_);

  rpc_.reset(new RpcTcpServer());
  if (!rpc_->start(cfg.rpcHost, cfg.rpcPort, reg_.get())) return false;

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

static std::string json_q(const std::string& s) {
  std::string o="\""; for(char c: s){ if(c=='"'||c=='\\') o.push_back('\\'); o.push_back(c);} o.push_back('"'); return o;
}

void Daemon::bindCommands(CommandRegistry& reg) {
  // Introspection
  reg.registerMethod("__introspect__", "List all methods.",
                     [this,&reg](const RpcRequest&)->RpcResult{ return {true, BuildIntrospectionJson(reg)}; });

  // Basic
  reg.registerMethod("rpc.ping", "Ping (returns pong).",
                     [](const RpcRequest&)->RpcResult{ return {true, "\"pong\""}; });
  reg.registerMethod("daemon.version", "Return daemon version.",
                     [](const RpcRequest&)->RpcResult{ return {true, "{\"name\":\"lfcd\",\"version\":\"0.4\",\"protocol\":\"jsonrpc2\"}"}; });

  // Enumerate hwmon
  reg.registerMethod("enumerate", "Enumerate hwmon sensors/pwms/fans.",
                     [this](const RpcRequest&)->RpcResult{
                       std::ostringstream os; os<<"{\"pwms\":[";
                       for (size_t i=0;i<hwmon_.pwms.size();++i){ if(i)os<<","; auto&p=hwmon_.pwms[i];
                         os<<"{\"hwmon\":"<<json_q(p.hwmon)<<",\"index\":"<<p.index<<",\"pwm\":"<<json_q(p.path_pwm)
                         <<",\"enable\":"<<json_q(p.path_en)<<",\"max\":"<<p.max_raw<<",\"w\":"<<(p.writable?"true":"false")<<"}";
                       }
                       os<<"],\"fans\":[";
                       for (size_t i=0;i<hwmon_.fans.size();++i){ if(i)os<<","; auto&f=hwmon_.fans[i];
                         os<<"{\"hwmon\":"<<json_q(f.hwmon)<<",\"index\":"<<f.index<<",\"input\":"<<json_q(f.path_input)<<"}";
                       }
                       os<<"],\"temps\":[";
                       for (size_t i=0;i<hwmon_.temps.size();++i){ if(i)os<<","; auto&t=hwmon_.temps[i];
                         os<<"{\"hwmon\":"<<json_q(t.hwmon)<<",\"index\":"<<t.index<<",\"input\":"<<json_q(t.path_input)<<"}";
                       }
                       os<<"]}";
                       return {true, os.str()};
                     });

  // Detection (aggressive)
  reg.registerMethod("detect.run", "Run aggressive detection (params: {level:int?}).",
                     [this](const RpcRequest& req)->RpcResult{
                       int level = 2;
                       auto pos=req.paramsJson.find("\"level\""); if(pos!=std::string::npos){ auto c=req.paramsJson.find(':',pos); if(c!=std::string::npos) level=std::stoi(req.paramsJson.substr(c+1)); }
                       auto res = engine_.detectAggressive(level);
                       return {true, res};
                     });

  // Channels
  reg.registerMethod("listChannels", "Return configured channels.",
                     [this](const RpcRequest&)->RpcResult{
                       auto v = engine_.list();
                       std::ostringstream os; os<<"[";
                       for (size_t i=0;i<v.size();++i){
                         if(i)os<<","; const auto& c=v[i];
                         os<<"{\"id\":"<<c.id<<",\"name\":"<<json_q(c.name)<<",\"mode\":"<<(c.mode==ChannelMode::Manual?0:1)
                         <<",\"pct\":"<<c.manualPercent<<",\"tau\":"<<c.hystTauSec<<",\"pwm\":"<<json_q(c.pwm.path_pwm)<<"}";
                       }
                       os<<"]"; return {true, os.str()};
                     });

  reg.registerMethod("createChannel", "Create channel (params:{name,pwm}).",
                     [this](const RpcRequest& req)->RpcResult{
                       auto npos=req.paramsJson.find("\"name\"");
                       auto ppos=req.paramsJson.find("\"pwm\"");
                       if(npos==std::string::npos||ppos==std::string::npos) return {false, jsonError("missing params")};
                       auto nq=req.paramsJson.find('"', npos+6); auto nqe=req.paramsJson.find('"', nq+1);
                       auto name=req.paramsJson.substr(nq+1, nqe-nq-1);
                       auto pq=req.paramsJson.find('"', ppos+5); auto pqe=req.paramsJson.find('"', pq+1);
                       auto pwmPath=req.paramsJson.substr(pq+1, pqe-pq-1);
                       HwmonPwm target{};
                       bool found=false; for (auto& p : hwmon_.pwms){ if(p.path_pwm==pwmPath){ target=p; found=true; break; } }
                       if (!found) return {false, jsonError("pwm not found")};
                       int id = engine_.create(name, target);
                       return {true, std::string("{\"id\":")+std::to_string(id)+"}"};
                     });

  reg.registerMethod("deleteChannel", "Delete channel (params:{id}).",
                     [this](const RpcRequest& req)->RpcResult{
                       auto pos=req.paramsJson.find("\"id\""); if(pos==std::string::npos) return {false,jsonError("missing id")};
                       auto c=req.paramsJson.find(':',pos); int id=std::stoi(req.paramsJson.substr(c+1));
                       bool ok = engine_.remove(id);
                       return {ok, ok?"{\"ok\":true}":jsonError("id not found")};
                     });

  reg.registerMethod("setChannelMode", "Set channel mode (params:{id,mode(0=manual,1=curve)}).",
                     [this](const RpcRequest& req)->RpcResult{
                       int id=-1, mode=0;
                       auto ip=req.paramsJson.find("\"id\"");   if(ip!=std::string::npos){ auto c=req.paramsJson.find(':',ip); id=std::stoi(req.paramsJson.substr(c+1)); }
                       auto mp=req.paramsJson.find("\"mode\""); if(mp!=std::string::npos){ auto c=req.paramsJson.find(':',mp); mode=std::stoi(req.paramsJson.substr(c+1)); }
                       bool ok = engine_.setMode(id, mode?ChannelMode::Curve:ChannelMode::Manual);
                       return {ok, ok?"{\"ok\":true}":jsonError("bad id")};
                     });

  reg.registerMethod("setChannelManual", "Set manual percent (params:{id,pct}).",
                     [this](const RpcRequest& req)->RpcResult{
                       int id=-1; double pct=30;
                       auto ip=req.paramsJson.find("\"id\"");   if(ip!=std::string::npos){ auto c=req.paramsJson.find(':',ip); id=std::stoi(req.paramsJson.substr(c+1)); }
                       auto pp=req.paramsJson.find("\"pct\"");  if(pp!=std::string::npos){ auto c=req.paramsJson.find(':',pp); pct=std::stod(req.paramsJson.substr(c+1)); }
                       bool ok = engine_.setManual(id, pct);
                       return {ok, ok?"{\"ok\":true}":jsonError("bad id")};
                     });

  reg.registerMethod("setChannelCurve", "Set curve (params:{id,points:[{x,y}...]}).",
                     [this](const RpcRequest& req)->RpcResult{
                       int id=-1; std::vector<CurvePt> pts;
                       auto ip=req.paramsJson.find("\"id\""); if(ip!=std::string::npos){ auto c=req.paramsJson.find(':',ip); id=std::stoi(req.paramsJson.substr(c+1)); }
                       auto pp=req.paramsJson.find("\"points\"");
                       if (pp!=std::string::npos) {
                         size_t b=req.paramsJson.find('[',pp), e=req.paramsJson.find(']',b);
                         if (b!=std::string::npos && e!=std::string::npos) {
                           std::string arr=req.paramsJson.substr(b+1,e-b-1);
                           size_t pos=0;
                           while (true) {
                             auto xk=arr.find("\"x\"",pos); if (xk==std::string::npos) break;
                             auto xc=arr.find(':',xk); auto yk=arr.find("\"y\"",xc); auto yc=arr.find(':',yk);
                             double x=std::stod(arr.substr(xc+1)); double y=std::stod(arr.substr(yc+1));
                             pts.push_back({x,y});
                             pos = arr.find('}', yc); if (pos==std::string::npos) break; ++pos;
                           }
                         }
                       }
                       bool ok = engine_.setCurve(id, std::move(pts));
                       return {ok, ok?"{\"ok\":true}":jsonError("bad id/points")};
                     });

  reg.registerMethod("setChannelHystTau", "Set hysteresis tau seconds (params:{id,tau}).",
                     [this](const RpcRequest& req)->RpcResult{
                       int id=-1; double tau=2.0;
                       auto ip=req.paramsJson.find("\"id\"");  if(ip!=std::string::npos){ auto c=req.paramsJson.find(':',ip); id=std::stoi(req.paramsJson.substr(c+1)); }
                       auto tp=req.paramsJson.find("\"tau\""); if(tp!=std::string::npos){ auto c=req.paramsJson.find(':',tp); tau=std::stod(req.paramsJson.substr(c+1)); }
                       bool ok = engine_.setHystTau(id, tau);
                       return {ok, ok?"{\"ok\":true}":jsonError("bad id")};
                     });

  // Engine control
  reg.registerMethod("engineStart", "Start control engine.",
                     [this](const RpcRequest&)->RpcResult{ engine_.start(); return {true, "{\"ok\":true}"}; });
  reg.registerMethod("engineStop", "Stop control engine.",
                     [this](const RpcRequest&)->RpcResult{ engine_.stop();  return {true, "{\"ok\":true}"}; });
}
