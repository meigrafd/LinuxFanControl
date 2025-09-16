/*
 * Linux Fan Control — Daemon (implementation)
 * - Initializes logging, telemetry, hwmon backend and RPC
 * - Applies LFCD_* env overrides (tick/deltaC/force)
 * - Drives the main control loop and optional detection
 * (c) 2025 LinuxFanControl contributors
 */

#include "Daemon.hpp"
#include "Config.hpp"
#include "RpcHandlers.hpp"
#include "FanControlImport.hpp"
#include "Detection.hpp"
#include "Log.hpp"
#include "Version.hpp"

#include <filesystem>
#include <fstream>
#include <thread>
#include <cstdlib>
#include <algorithm>    // std::clamp
#include <unistd.h>     // getpid()

namespace lfc {

// ----------- helpers -----------

int Daemon::getenv_int(const char* key, int def) {
    if (const char* s = std::getenv(key)) {
        try { return std::stoi(s); } catch (...) {}
    }
    return def;
}

double Daemon::getenv_double(const char* key, double def) {
    if (const char* s = std::getenv(key)) {
        try { return std::stod(s); } catch (...) {}
    }
    return def;
}

std::string Daemon::ensure_profile_filename(const std::string& name) {
    if (name.empty()) return name;
    if (name.size() >= 5 && name.substr(name.size() - 5) == ".json") return name;
    return name + ".json";
}

// ----------- lifecycle -----------

Daemon::Daemon() = default;
Daemon::~Daemon() = default;

bool Daemon::init(DaemonConfig& cfg, bool debugCli, const std::string& cfgPath, bool foreground) {
    cfg_ = cfg;
    debug_ = debugCli || cfg.debug;
    configPath_ = cfgPath;
    foreground_ = foreground;

    if (cfg_.tickMs <= 0) cfg_.tickMs = 25;
    if (cfg_.deltaC < 0.0) cfg_.deltaC = 0.5;
    if (cfg_.forceTickMs <= 0) cfg_.forceTickMs = 1000;

    tickMs_      = std::clamp(getenv_int("LFCD_TICK_MS",       cfg_.tickMs),      5,   1000);
    deltaC_      = std::clamp(getenv_double("LFCD_DELTA_C",    cfg_.deltaC),      0.0, 10.0);
    forceTickMs_ = std::clamp(getenv_int("LFCD_FORCE_TICK_MS", cfg_.forceTickMs), 100, 10000);

    const std::string logPath = cfg_.logfile.empty() ? std::string("/tmp/daemon_lfc.log") : cfg_.logfile;
    Logger::instance().configure(logPath, 0, 0, debug_);
    LFC_LOGI("lfcd v%s starting", LFCD_VERSION);

    try {
        if (!configPath_.empty()) std::filesystem::create_directories(std::filesystem::path(configPath_).parent_path());
        if (!cfg_.profilesDir.empty()) std::filesystem::create_directories(cfg_.profilesDir);
        if (!cfg_.pidfile.empty()) {
            std::filesystem::create_directories(std::filesystem::path(cfg_.pidfile).parent_path());
        }
    } catch (...) {}

    {
        const std::string shm = cfg_.shmPath.empty() ? std::string("/dev/shm/lfc_telemetry") : cfg_.shmPath;
        if (!telemetry_.init(shm)) {
            LFC_LOGW("telemetry: init failed: %s", shm.c_str());
        }
    }

    hwmon_ = Hwmon::scan();

    engine_ = std::make_unique<Engine>();
    engine_->setHwmonView(hwmon_.temps, hwmon_.fans, hwmon_.pwms);
    engine_->attachTelemetry(&telemetry_);

    rpcRegistry_ = std::make_unique<CommandRegistry>();
    BindDaemonRpcCommands(*this, *rpcRegistry_);

    {
        const std::string host = cfg_.host.empty() ? std::string("127.0.0.1") : cfg_.host;
        const unsigned short port = static_cast<unsigned short>(cfg_.port > 0 ? cfg_.port : 8777);
        rpcServer_ = std::make_unique<RpcTcpServer>(*this, host, port, debug_);
        if (!rpcServer_->start(rpcRegistry_.get())) {
            LFC_LOGE("rpc: failed to start on %s:%u", host.c_str(), (unsigned)port);
            return false;
        }
        LFC_LOGI("rpc: listening on %s:%u", host.c_str(), (unsigned)port);
    }

    // Write PID file (best-effort)
    if (!cfg_.pidfile.empty()) {
        try {
            std::ofstream pf(cfg_.pidfile, std::ios::trunc);
            if (pf) {
                pf << getpid() << "\n";
            } else {
                LFC_LOGW("pidfile: cannot write %s", cfg_.pidfile.c_str());
            }
        } catch (...) {
            LFC_LOGW("pidfile: exception while writing %s", cfg_.pidfile.c_str());
        }
    }

    if (!cfg_.profilesDir.empty() && !cfg_.profileName.empty()) {
        const auto full = (std::filesystem::path(cfg_.profilesDir) / ensure_profile_filename(cfg_.profileName)).string();
        if (std::filesystem::exists(full)) {
            if (!applyProfileFile(full)) {
                LFC_LOGW("profile: failed to load '%s' — waiting for detection/import", full.c_str());
            } else {
                LFC_LOGI("profile: loaded '%s'", full.c_str());
            }
        } else {
            LFC_LOGI("no profile present at %s — waiting for detection/import", full.c_str());
        }
    } else {
        LFC_LOGI("no profile configured — waiting for detection/import");
    }

    {
        const std::string tj = engine_->telemetryJson();
        (void)telemetry_.update(tj);
    }

    running_.store(true, std::memory_order_relaxed);
    stop_.store(false, std::memory_order_relaxed);
    return true;
}

void Daemon::runLoop() {
    using clock = std::chrono::steady_clock;

    auto lastForce = clock::now();
    auto nextTick  = clock::now();

    while (running_.load(std::memory_order_relaxed) && !stop_.load(std::memory_order_relaxed)) {
        const int tickMs  = tickMs_;
        const int forceMs = forceTickMs_;
        const double dC   = deltaC_;

        auto now = clock::now();
        bool force = (std::chrono::duration_cast<std::chrono::milliseconds>(now - lastForce).count() >= forceMs);

        if (force || now >= nextTick) {
            (void)engine_->tick(dC);
            if (force) lastForce = now;
            nextTick = now + std::chrono::milliseconds(tickMs);
        }

        if (detection_ && detection_->running()) {
            detection_->poll();
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

void Daemon::shutdown() {
    stop_.store(true, std::memory_order_relaxed);
    running_.store(false, std::memory_order_relaxed);

    if (detection_) {
        detection_->abort();
        detection_.reset();
    }
    if (rpcServer_) rpcServer_->stop();
    if (engine_) engine_->enable(false);

    // Remove PID file
    if (!cfg_.pidfile.empty()) {
        std::error_code ec;
        std::filesystem::remove(cfg_.pidfile, ec);
    }
}

// ----------- telemetry / engine control -----------

bool Daemon::telemetryGet(std::string& out) const {
    return telemetry_.get(out);
}

bool Daemon::engineControlEnabled() const {
    return engine_ ? engine_->enabled() : false;
}

void Daemon::engineEnable(bool on) {
    if (engine_) engine_->enable(on);
}

void Daemon::setEngineDeltaC(double v) {
    deltaC_ = std::clamp(v, 0.0, 10.0);
}

void Daemon::setEngineForceTickMs(int v) {
    forceTickMs_ = std::clamp(v, 100, 10000);
}

void Daemon::setEngineTickMs(int v) {
    tickMs_ = std::clamp(v, 5, 1000);
}

// ----------- detection passthrough -----------

bool Daemon::detectionStart() {
    if (detection_ && detection_->running()) return false;
    hwmon_ = Hwmon::scan();
    if (engine_) engine_->setHwmonView(hwmon_.temps, hwmon_.fans, hwmon_.pwms);

    DetectionConfig dcfg;
    detection_ = std::make_unique<Detection>(hwmon_, dcfg);
    detection_->start();
    return true;
}

void Daemon::detectionAbort() {
    if (detection_) detection_->abort();
}

Daemon::DetectionStatus Daemon::detectionStatus() const {
    DetectionStatus s{};
    if (!detection_) return s;
    auto st = detection_->status();
    s.running = st.running;
    s.currentIndex = st.currentIndex;
    s.total = st.total;
    s.phase = st.phase;
    return s;
}

std::vector<int> Daemon::detectionResults() const {
    if (!detection_) return {};
    return detection_->results();
}

// ----------- profiles -----------

bool Daemon::applyProfile(const Profile& p) {
    if (!engine_) return false;
    engine_->applyProfile(p);
    return true;
}

bool Daemon::applyProfileIfValid(const std::string& path) {
    Profile p;
    std::string perr;
    if (p.loadFromFile(path, &perr)) {
        engine_->applyProfile(p);
        LFC_LOGI("profile: applied '%s' (native)", path.c_str());
        return true;
    }
    return false;
}

bool Daemon::applyProfileFile(const std::string& path) {
    if (applyProfileIfValid(path)) return true;

    Profile p;
    std::string ierr;
    if (FanControlImport::LoadAndMap(path, hwmon_.temps, hwmon_.pwms, p, ierr)) {
        engine_->applyProfile(p);
        LFC_LOGI("profile: imported & applied '%s' (FanControl.Releases)", path.c_str());
        return true;
    }
    LFC_LOGW("profile: import failed for '%s': %s", path.c_str(), ierr.c_str());
    return false;
}

// ----------- RPC registry -----------

std::vector<CommandInfo> Daemon::listRpcCommands() const {
    if (!rpcRegistry_) return {};
    return rpcRegistry_->list();
}

Profile Daemon::currentProfile() const {
    return engine_ ? engine_->currentProfile() : Profile{};
}

} // namespace lfc
