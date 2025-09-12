// Engine.cpp — control loop & mutators
#include "Engine.h"
#include "Hwmon.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <thread>

// --- public API ----------------------------------------------------------

void Engine::setChannels(std::vector<Channel> chs) {
    std::lock_guard<std::mutex> lk(mu_);
    chans_ = std::move(chs);
}

std::vector<Channel> Engine::snapshot() {
    std::lock_guard<std::mutex> lk(mu_);
    return chans_;
}

bool Engine::start() {
    if (running_) return true;
    running_ = true;
    th_ = std::thread([this]{ this->loop(); });
    return true;
}

void Engine::stop() {
    running_ = false;
    if (th_.joinable()) th_.join();
}

void Engine::updateChannelMode(const std::string& id, const std::string& mode) {
    std::lock_guard<std::mutex> lk(mu_);
    for (auto& c : chans_) if (c.id == id) { c.mode = mode; break; }
}

void Engine::updateChannelManual(const std::string& id, double pct) {
    std::lock_guard<std::mutex> lk(mu_);
    for (auto& c : chans_) if (c.id == id) { c.manual_pct = std::clamp(pct, 0.0, 100.0); break; }
}

void Engine::updateChannelCurve(const std::string& id, const std::vector<CurvePoint>& pts) {
    std::lock_guard<std::mutex> lk(mu_);
    for (auto& c : chans_) if (c.id == id) { c.curve = pts; break; }
}

void Engine::updateChannelHystTau(const std::string& id, double hyst, double tau) {
    std::lock_guard<std::mutex> lk(mu_);
    for (auto& c : chans_) if (c.id == id) { c.hyst_c = std::max(0.0, hyst); c.tau_s = std::max(0.0, tau); break; }
}

// --- helpers -------------------------------------------------------------

static double clamp01(double v) { return std::max(0.0, std::min(1.0, v)); }

// piecewise-linear evaluation
double Engine::evalCurve(const std::vector<CurvePoint>& pts, double x) {
    if (pts.empty()) return 0.0;
    std::vector<CurvePoint> cp = pts;
    std::sort(cp.begin(), cp.end(), [](auto& a, auto& b){ return a.x < b.x; });

    if (x <= cp.front().x) return cp.front().y;
    if (x >= cp.back().x)  return cp.back().y;

    for (size_t i = 1; i < cp.size(); ++i) {
        const auto& a = cp[i-1]; const auto& b = cp[i];
        if (x >= a.x && x <= b.x) {
            const double t = (b.x == a.x) ? 1.0 : (x - a.x) / (b.x - a.x);
            return a.y + t * (b.y - a.y);
        }
    }
    return cp.back().y;
}

// --- main loop -----------------------------------------------------------

void Engine::loop() {
    using namespace std::chrono_literals;
    while (running_) {
        // take a working copy to minimize lock duration while IO happens
        std::vector<Channel> work;
        {
            std::lock_guard<std::mutex> lk(mu_);
            work = chans_;
        }

        for (auto& c : work) {
            // read sensor
            const double t = Hwmon::readTempC(c.sensor_path);
            if (std::isfinite(t)) c.last_temp = t;

            // compute duty
            double duty = 0.0;
            if (c.mode == "Manual") {
                duty = c.manual_pct;
            } else {
                const double x = std::isfinite(c.last_temp) ? c.last_temp : t;
                duty = evalCurve(c.curve, x);
            }

            // clamp to limits and write
            duty = std::max(c.min_pct, std::min(c.max_pct, duty));
            c.last_out = duty;

            PwmDevice dev;
            dev.pwm_path    = c.pwm_path;
            dev.enable_path = c.enable_path;
            dev.tach_path   = c.tach_path;
            dev.min_pct     = c.min_pct;
            dev.max_pct     = c.max_pct;
            Hwmon::setPwmPercent(dev, duty, nullptr);
        }

        // write back telemetry (temp/out) to shared model
        {
            std::lock_guard<std::mutex> lk(mu_);
            // merge by id
            for (auto& w : work) {
                for (auto& c : chans_) {
                    if (c.id == w.id) {
                        c.last_temp = w.last_temp;
                        c.last_out  = w.last_out;
                        break;
                    }
                }
            }
        }

        std::this_thread::sleep_for(200ms);
    }
}
