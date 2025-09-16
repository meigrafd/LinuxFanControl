/*
 * Linux Fan Control — Daemon (implementation)
 * (c) 2025 LinuxFanControl contributors
 */
#include "Daemon.hpp"
#include "RpcHandlers.hpp"
#include "Hwmon.hpp"
#include "Log.hpp"
#include "Config.hpp"

#include <filesystem>
#include <thread>
#include <chrono>
#include <cstdio>
#include <sys/stat.h>
#include <unistd.h>

using namespace std::chrono_literals;
namespace fs = std::filesystem;

namespace lfc {

Daemon::Daemon() = default;
Daemon::~Daemon() {
    shutdown();
}

static bool file_parent_dir(const std::string& p, std::string& outDir) {
    try {
        fs::path path(p);
        outDir = path.parent_path().string();
        return true;
    } catch (...) {
        return false;
    }
}

bool Daemon::ensureDir(const std::string& path) {
    std::error_code ec;
    fs::create_directories(path, ec);
    return !ec;
}

bool Daemon::writePidFile(const std::string& path) {
    pidFile_ = path;
    std::FILE* f = std::fopen(path.c_str(), "w");
    if (!f) return false;
    std::fprintf(f, "%d\n", (int)getpid());
    std::fclose(f);
    return true;
}

void Daemon::removePidFile() {
    if (!pidFile_.empty()) {
        std::error_code ec;
        fs::remove(pidFile_, ec);
        pidFile_.clear();
    }
}

static HwBackend parse_backend(const std::string& s) {
    if (s == "sysfs") return HwBackend::Sysfs;
    if (s == "libsensors") return HwBackend::Libsensors;
    return HwBackend::Auto;
}

bool Daemon::init(DaemonConfig& cfg, bool /*debugCli*/, const std::string& cfgPath, bool /*foreground*/) {
    cfg_ = cfg;
    configPath_ = cfgPath;

    // apply loop tuning from config
    setEngineTickMs(cfg_.tickMs);
    setEngineDeltaC(cfg_.deltaC);
    setEngineForceTickMs(cfg_.forceTickMs);

    // hwmon scan (backend from config)
    hwmon_ = Hwmon::scan(parse_backend(cfg_.sensorsBackend));

    // shm init
    if (!engine_.initShm(cfg_.shmPath)) {
        LFC_LOGW("SHM init failed at %s", cfg_.shmPath.c_str());
    }

    engine_.setSnapshot(hwmon_);
    engine_.enableControl(true);
    engine_.start();

    // RPC registry + TCP server
    reg_ = std::make_unique<CommandRegistry>();
    BindDaemonRpcCommands(*this, *reg_);

    rpc_ = std::make_unique<RpcTcpServer>(*this, cfg_.host, static_cast<unsigned short>(cfg_.port), cfg_.debug);
    (void)rpc_->start(reg_.get());

    // optional pidfile
    if (!cfg_.pidfile.empty()) {
        std::string dir;
        if (file_parent_dir(cfg_.pidfile, dir)) ensureDir(dir);
        (void)writePidFile(cfg_.pidfile);
    }

    return true;
}

void Daemon::runLoop() {
    running_.store(true);
    while (running_.load()) {
        auto t0 = std::chrono::steady_clock::now();
        pumpOnce();
        auto t1 = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
        int sleepMs = tickMs_ - static_cast<int>(elapsed);
        if (sleepMs < 0) sleepMs = 0;
        std::this_thread::sleep_for(std::chrono::milliseconds(sleepMs));
    }
    shutdown();
}

void Daemon::shutdown() {
    if (rpc_) {
        rpc_->stop();
        rpc_.reset();
    }
    engine_.stop();
    removePidFile();
}

void Daemon::pumpOnce() {
    engine_.tick();
    std::lock_guard<std::mutex> lk(detMu_);
    if (detection_) detection_->poll();
}

RpcResult Daemon::dispatch(const std::string& method, const std::string& paramsJson) {
    if (!reg_) return {false, "{\"ok\":false,\"code\":-32601,\"error\":\"no registry\"}"};
    RpcRequest req{method, "", paramsJson};
    return reg_->call(req);
}

std::vector<CommandInfo> Daemon::listRpcCommands() const {
    if (!reg_) return {};
    return reg_->list();
}

bool Daemon::applyProfileIfValid(const std::string& profilePath) {
    Profile p;
    std::string err;
    if (!p.loadFromFile(profilePath, &err)) {
        LFC_LOGE("Profile load failed: %s (%s)", profilePath.c_str(), err.c_str());
        return false;
    }
    engine_.applyProfile(p);
    LFC_LOGI("Applied profile: %s", profilePath.c_str());
    return true;
}

// detection proxies
bool Daemon::detectionStart() {
    std::lock_guard<std::mutex> lk(detMu_);
    if (detection_ && detection_->running()) return false;
    DetectionConfig dc; // defaults
    detection_ = std::make_unique<Detection>(hwmon_, dc);
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
