/*
 * Linux Fan Control — Daemon (implementation)
 * - Lifecycle, SHM, PID handling, light mainloop
 * (c) 2025 LinuxFanControl contributors
 */
#include "Daemon.hpp"
#include "RpcHandlers.hpp"
#include "Detection.hpp"
#include "Config.hpp"
#include "Log.hpp"
#include "Hwmon.hpp"
#include "Version.hpp"

#include <fstream>
#include <filesystem>
#include <chrono>
#include <thread>
#include <cstdlib>
#include <unistd.h>

namespace lfc {

Daemon::Daemon() = default;
Daemon::~Daemon() { shutdown(); }

bool Daemon::ensureDir(const std::string& path) {
    std::error_code ec;
    auto dir = std::filesystem::path(path).parent_path();
    if (dir.empty()) return true;
    std::filesystem::create_directories(dir, ec);
    return !ec;
}

bool Daemon::writePidFile(const std::string& path) {
    if (!ensureDir(path)) return false;
    std::ofstream f(path, std::ios::trunc);
    if (!f) return false;
    f << getpid() << "\n";
    pidFile_ = path;
    return true;
}

void Daemon::removePidFile() {
    if (pidFile_.empty()) return;
    std::error_code ec;
    std::filesystem::remove(pidFile_, ec);
    pidFile_.clear();
}

bool Daemon::applyProfileIfValid(const std::string& profilePath) {
    std::string err;
    Profile p;
    if (!ProfileIO::Load(profilePath, p, err)) {
        LFC_LOGW("profile load failed: %s", err.c_str());
        return false;
    }
    if (!ProfileIO::Validate(p, hwmon_, err)) {
        LFC_LOGW("profile invalid: %s", err.c_str());
        return false;
    }
    engine_.applyProfile(p);
    LFC_LOGI("profile applied: %s", profilePath.c_str());
    return true;
}

bool Daemon::init(DaemonConfig& cfg, bool debugCli, const std::string& cfgPath) {
    configPath_ = cfgPath;
    cfg_ = cfg;

    Logger::instance().configure(cfg.log.file, cfg.log.maxBytes, cfg.log.rotateCount, cfg.log.debug || debugCli);
    if (cfg.log.debug || debugCli) Logger::instance().setLevel(LogLevel::Debug);

    std::string primary = cfg.pidFile.empty() ? std::string("/run/lfcd.pid") : cfg.pidFile;
    if (!writePidFile(primary)) {
        std::string fallback{"/tmp/lfcd.pid"};
        if (writePidFile(fallback)) {
            cfg.pidFile = fallback;
            cfg_ = cfg;
            std::string e;
            (void)Config::Save(cfgPath, cfg_, e);
            LFC_LOGI("pidfile fallback to %s (persisted)", fallback.c_str());
        } else {
            LFC_LOGW("pidfile create failed (primary: %s, fallback: %s)", primary.c_str(), fallback.c_str());
        }
    }

    hwmon_ = Hwmon::scan();
    engine_.setSnapshot(hwmon_);
    engine_.initShm(cfg.shm.path);

    std::filesystem::path profPath = std::filesystem::path(cfg.profiles.dir) / cfg.profiles.active;
    if (std::filesystem::exists(profPath)) {
        applyProfileIfValid(profPath.string());
    } else {
        LFC_LOGI("no profile present at %s — awaiting detection/import via RPC", profPath.string().c_str());
        engine_.enableControl(false);
    }

    reg_ = std::make_unique<CommandRegistry>();
    BindDaemonRpcCommands(*this, *reg_);

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
    {
        std::lock_guard<std::mutex> lk(detMu_);
        if (detection_) detection_->abort();
    }
    engine_.stop();
    removePidFile();
}

void Daemon::runLoop() {
    using clock = std::chrono::steady_clock;
    int tick_ms = 25;
    if (const char* ev = ::getenv("LFC_TICK_MS")) {
        int v = std::atoi(ev);
        if (v >= 5 && v <= 1000) tick_ms = v;
    }
    auto next = clock::now();
    while (running_) {
        next += std::chrono::milliseconds(tick_ms);
        pumpOnce();
        auto now = clock::now();
        if (now < next) std::this_thread::sleep_until(next);
        else next = now;
    }
}

void Daemon::pumpOnce(int /*timeoutMs*/) {
    // Temp gating is handled inside Engine (or keep your current gating if present).
    engine_.tick();
    std::lock_guard<std::mutex> lk(detMu_);
    if (detection_) detection_->poll();
}

RpcResult Daemon::dispatch(const std::string& method, const std::string& paramsJson) {
    if (!reg_) return {false, "{\"method\":\"dispatch\",\"success\":false,\"error\":{\"code\":-32601,\"message\":\"no registry\"}}"};
    RpcRequest req{method, "", paramsJson};
    return reg_->call(req);
}

std::vector<CommandInfo> Daemon::listRpcCommands() const {
    if (!reg_) return {};
    return reg_->list();
}

// detection proxies
bool Daemon::detectionStart() {
    std::lock_guard<std::mutex> lk(detMu_);
    if (detection_ && detection_->running()) return false;
    detection_ = std::unique_ptr<Detection>(new Detection(hwmon_));
    detection_->start();
    return true;
}
void Daemon::detectionAbort() {
    std::lock_guard<std::mutex> lk(detMu_);
    if (detection_) detection_->abort();
}
Detection::Status Daemon::detectionStatus() const {
    std::lock_guard<std::mutex> lk(detMu_);
    if (!detection_) return Detection::Status{};
    return detection_->status();
}
std::vector<int> Daemon::detectionResults() const {
    std::lock_guard<std::mutex> lk(detMu_);
    if (!detection_) return {};
    return detection_->results();
}

} // namespace lfc
