/*
 * Linux Fan Control — Daemon (implementation)
 * (c) 2025 LinuxFanControl contributors
 */
#include "include/Utils.hpp"
#include "include/Daemon.hpp"
#include "include/Engine.hpp"
#include "include/Hwmon.hpp"
#include "include/GpuMonitor.hpp"
#include "include/ShmTelemetry.hpp"
#include "include/Detection.hpp"
#include "include/Log.hpp"
#include "include/VendorMapping.hpp"
#include "rpc/ImportJobs.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <thread>

namespace fs = std::filesystem;

namespace lfc {

Daemon::Daemon()
: configPath_(),
  telemetry_(nullptr)
{
    LOG_TRACE("daemon: ctor");
}

Daemon::Daemon(std::string cfgPath)
: configPath_(std::move(cfgPath)),
  telemetry_(nullptr)
{}

Daemon::~Daemon() {
    LOG_TRACE("daemon: dtor");
    shutdown();
}

bool Daemon::init(const DaemonConfig& cfg, bool debugCli) {
    LOG_INFO("daemon: init start");
    cfg_   = cfg;
    debug_ = debugCli || cfg.debug;

    // Optional vendor mapping override (hot-reloadable)
    if (!cfg_.vendorMapPath.empty()) {
        VendorMapping::instance().setOverridePath(cfg_.vendorMapPath);
        LOG_INFO("daemon: vendor map override: %s", cfg_.vendorMapPath.c_str());
    }
    {
        using WM = VendorMapping::WatchMode;
        WM mode = (cfg_.vendorMapWatchMode == "inotify") ? WM::Inotify : WM::MTime;
        VendorMapping::instance().setWatchMode(mode, cfg_.vendorMapThrottleMs);
        LOG_INFO("daemon: vendor map watch mode=%s throttleMs=%d",
                 cfg_.vendorMapWatchMode.c_str(), cfg_.vendorMapThrottleMs);
    }

    // Construct telemetry with path coming from Config.* (no defaults here)
    telemetry_ = std::make_unique<ShmTelemetry>(cfg_.shmPath);
    LOG_INFO("daemon: telemetry shm at: %s", cfg_.shmPath.c_str());

    // One-time hwmon inventory discovery; values will be read live later.
    refreshHwmon();
    LOG_INFO("daemon: hwmon snapshot temps=%zu fans=%zu pwms=%zu", hwmon_.temps.size(), hwmon_.fans.size(), hwmon_.pwms.size());
    rememberOriginalEnables();

    // Engine
    engine_ = std::make_unique<Engine>();
    engine_->setHwmonView(hwmon_.temps, hwmon_.fans, hwmon_.pwms);
    //engine_->applyProfile(profile_);
    LOG_DEBUG("daemon: engine ready");

    // Engine disabled by default until a valid profile is loaded
    setEnabled(false);

    // Try to load active profile from disk and enable engine on success.
    try {
        const std::string profPath = profilePathForName(cfg_.profileName);
        std::error_code ec;
        if (!profPath.empty() && fs::exists(profPath, ec) && !ec) {
            Profile loaded = loadProfileFromFile(profPath);
            applyProfile(loaded);
            setEnabled(true);
            LOG_INFO("daemon: loaded profile '%s' -> engine enabled", cfg_.profileName.c_str());
        } else {
            LOG_INFO("daemon: no profile file yet ('%s'); engine stays disabled", profPath.c_str());
        }
    } catch (const std::exception& ex) {
        setEnabled(false);
        LOG_WARN("daemon: failed to load profile '%s': %s (engine disabled)",
                 cfg_.profileName.c_str(), ex.what());
    }

    // One-shot GPU inventory (metrics will be refreshed lightly in the loop)
    gpus_ = GpuMonitor::snapshot();

    // RPC
    rpcRegistry_ = std::make_unique<CommandRegistry>();
    rpcServer_   = std::make_unique<RpcTcpServer>(*this,
                     cfg_.host.empty() ? std::string("127.0.0.1") : cfg_.host,
                     static_cast<unsigned short>(cfg_.port > 0 ? cfg_.port : 8777),
                     debug_);
    if (!rpcServer_->start(rpcRegistry_.get())) {
        LOG_ERROR("daemon: rpc server start failed");
        return false;
    }
    LOG_INFO("daemon: init done (rpc on %s:%u)", cfg_.host.c_str(), (unsigned) (cfg_.port>0?cfg_.port:8777));

    running_.store(true, std::memory_order_relaxed);
    return true;
}

void Daemon::runLoop() {
    LOG_INFO("daemon: runLoop enter");
    using clock = std::chrono::steady_clock;

    // Initial schedule anchors
    auto nextTick  = clock::now();  // engine tick due
    auto lastForce = clock::now();  // last forced telemetry publish
    auto lastGpu   = clock::now();  // last lightweight GPU metrics refresh
    auto lastHwmon = clock::now();  // last lightweight hwmon housekeeping

    // NOTE: No periodic hwmon rescan here. Inventory is static during runtime.
    //       Sensors/controls are read live by Engine/telemetry helpers.

    constexpr int kSleepMinMs     = 1;   // avoid 0ms busy spin
    constexpr int kSleepMaxMs     = 50;  // keep responsiveness for RPC & state changes

    while (running_.load(std::memory_order_relaxed) && !stop_.load(std::memory_order_relaxed)) {
        auto now = clock::now();

        // Engine tick (curve evaluation & PWM writes)
        if (now >= nextTick) {
            if (enabled_.load(std::memory_order_relaxed) && engine_) {
                (void)engine_->tick(cfg_.deltaC);
            }
            nextTick = now + std::chrono::milliseconds(cfg_.tickMs);
        }

        // ---- Telemetry publish at forceTick cadence -------------------------
        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - lastForce).count() >= cfg_.forceTickMs) {
            publishTelemetry();
            lastForce = now;
        }

        // ---- Lightweight GPU metrics refresh --------------------------------
        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - lastGpu).count() >= cfg_.gpuRefreshMs) {
            GpuMonitor::refreshMetrics(gpus_);
            lastGpu = now;
        }

        // ---- Lightweight hwmon housekeeping ---------------------------------
        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - lastHwmon).count() >= cfg_.hwmonRefreshMs) {
            Hwmon::refreshValues(hwmon_);
            lastHwmon = now;
        }

        // ---------------------- dynamic sleep ----------------------------
        // Sleep until the earliest upcoming due time among all periodic tasks,
        // clamped into [kSleepMinMs, kSleepMaxMs].
        auto dueTick   = nextTick;
        auto dueForce  = lastForce + std::chrono::milliseconds(cfg_.forceTickMs);
        auto dueGpu    = lastGpu   + std::chrono::milliseconds(cfg_.gpuRefreshMs);
        auto dueHwmon  = lastHwmon + std::chrono::milliseconds(cfg_.hwmonRefreshMs);

        auto nextDue   = std::min({dueTick, dueForce, dueGpu, dueHwmon});
        // If already due/past-due, sleep the minimum to yield.
        long sleepMs   = std::chrono::duration_cast<std::chrono::milliseconds>(nextDue > now ? (nextDue - now) : std::chrono::milliseconds(kSleepMinMs)).count();

        if (sleepMs < kSleepMinMs) sleepMs = kSleepMinMs;
        if (sleepMs > kSleepMaxMs) sleepMs = kSleepMaxMs;

        std::this_thread::sleep_for(std::chrono::milliseconds(sleepMs));
    }

    LOG_INFO("daemon: run loop end");
}

void Daemon::shutdown() {
    LOG_INFO("daemon: shutdown");
    if (!running_.exchange(false)) return;

    if (rpcServer_) {
        LOG_DEBUG("daemon: stopping rpc");
        rpcServer_->stop();
        rpcServer_.reset();
    }
    rpcRegistry_.reset();

    if (detectRunning_.load(std::memory_order_relaxed)) {
        detectionRequestStop();
        if (detectThread_.joinable()) detectThread_.join();
    }

    engine_.reset();
    detection_.reset();
    telemetry_.reset();

    restoreOriginalEnables();
    LOG_INFO("daemon: shutdown complete");
}

/* -------------------------- internal helpers ------------------------------- */

void Daemon::refreshHwmon() {
    LOG_TRACE("daemon: refreshHwmon");
    hwmon_ = Hwmon::scan();
}

void Daemon::refreshGpus() {
    LOG_TRACE("daemon: refreshGpus");
    gpus_ = GpuMonitor::snapshot();
}

void Daemon::publishTelemetry() {
    LOG_TRACE("daemon: publishTelemetry");
    if (telemetry_) {
        (void)telemetry_->publish(
            hwmon_,            // inventory only; values read live inside
            gpus_,
            profile_,
            enabled_.load(std::memory_order_relaxed),
            nullptr);
    }
}

void Daemon::rememberOriginalEnables() {
    LOG_DEBUG("daemon: rememberOriginalEnables");
    origPwmEnable_.clear();
    for (const auto& p : hwmon_.pwms) {
        int mode = 2;
        if (auto v = Hwmon::readEnable(p)) mode = *v;
        origPwmEnable_.push_back({p.path_enable, mode});
    }
}

void Daemon::restoreOriginalEnables() {
    LOG_DEBUG("daemon: restoreOriginalEnables");
    for (const auto& it : origPwmEnable_) {
        std::ofstream f(it.first);
        if (f) { f << it.second; }
    }
    origPwmEnable_.clear();
}

bool Daemon::telemetryGet(std::string& outJson) const {
    LOG_TRACE("daemon: telemetryGet");
    auto j = ShmTelemetry::buildJson(hwmon_, gpus_, profile_, enabled_.load(std::memory_order_relaxed));
    outJson = j.dump(2);
    return true;
}

std::string Daemon::profilesPath() const {
    return lfc::util::expandUserPath(cfg_.profilesPath);
}

std::string Daemon::profilePathForName(const std::string& name) const {
    const std::string base = profilesPath();
    fs::path dir(base);
    fs::path file = (name.empty() ? cfg_.profileName : name) + std::string(".json");
    return (dir / file).string();
}

void Daemon::applyProfile(const Profile& p) {
    profile_ = p;
    if (engine_) engine_->applyProfile(profile_);
}

// ----- Detection control -------------------------------------------------------

bool Daemon::detectionStart() {
    LOG_INFO("daemon: detectionStart");
    if (detectRunning_.load(std::memory_order_relaxed)) return false;

    DetectionConfig dcfg{};
    detection_ = std::make_unique<Detection>(dcfg);

    {
        std::lock_guard<std::mutex> lk(detectMtx_);
        detectResult_ = DetectResult{};
    }

    detectRunning_.store(true, std::memory_order_relaxed);
    detectThread_ = std::thread([this]{
        LOG_DEBUG("daemon: detection thread started");
        DetectResult res;
        if (!detection_) {
            res.ok = false;
            res.error = "no detection object";
        } else {
            detection_->runAutoDetect(hwmon_, res);
        }
        {
            std::lock_guard<std::mutex> lk(detectMtx_);
            detectResult_ = std::move(res);
        }
        detectRunning_.store(false, std::memory_order_relaxed);
        LOG_INFO("daemon: detection thread finished (ok=%s)", detectResult_.ok? "true":"false");
    });

    return true;
}

bool Daemon::detectionStatus(DetectResult& out) const {
    LOG_TRACE("daemon: detectionStatus");
    std::lock_guard<std::mutex> lk(detectMtx_);
    out = detectResult_;
    out.ok = out.ok && !detectRunning_.load(std::memory_order_relaxed);
    return true;
}

void Daemon::detectionRequestStop() {
    LOG_INFO("daemon: detectionRequestStop");
    if (detection_) detection_->requestStop();
}

} // namespace lfc
