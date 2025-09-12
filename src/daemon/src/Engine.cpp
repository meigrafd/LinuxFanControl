#include "Engine.h"
#include <chrono>
#include <algorithm>

using namespace std::chrono_literals;

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
    th_ = std::thread(&Engine::loop, this);
    return true;
}
void Engine::stop() {
    if (!running_) return;
    running_ = false;
    if (th_.joinable()) th_.join();
}

void Engine::updateChannelMode(const std::string& id, const std::string& mode) {
    std::lock_guard<std::mutex> lk(mu_);
    for (auto& c : chans_) if (c.id == id) { c.mode = mode; break; }
}
void Engine::updateChannelManual(const std::string& id, double pct) {
    std::lock_guard<std::mutex> lk(mu_);
    for (auto& c : chans_) if (c.id == id) { c.manual_pct = pct; break; }
}
void Engine::updateChannelCurve(const std::string& id, const std::vector<CurvePoint>& pts) {
    std::lock_guard<std::mutex> lk(mu_);
    for (auto& c : chans_) if (c.id == id) { c.curve = pts; break; }
}
void Engine::updateChannelHystTau(const std::string& id, double hyst, double tau) {
    std::lock_guard<std::mutex> lk(mu_);
    for (auto& c : chans_) if (c.id == id) { c.hyst_c = hyst; c.tau_s = tau; break; }
}

double Engine::evalCurve(const std::vector<CurvePoint>& pts, double x) {
    if (pts.empty()) return 0.0;
    if (x <= pts.front().x) return pts.front().y;
    if (x >= pts.back().x)  return pts.back().y;
    for (size_t i=1;i<pts.size();++i) {
        if (x <= pts[i].x) {
            double x0=pts[i-1].x, y0=pts[i-1].y, x1=pts[i].x, y1=pts[i].y;
            double t = (x - x0) / (x1 - x0);
            return y0 + t*(y1-y0);
        }
    }
    return pts.back().y;
}

void Engine::loop() {
    while (running_) {
        {
            std::lock_guard<std::mutex> lk(mu_);
            for (auto& c : chans_) {
                auto t = Hwmon::readTempC(c.sensor_path);
                if (t) c.last_temp = *t;
                double duty = 0.0;
                if (c.mode == "Manual") duty = c.manual_pct;
                else duty = evalCurve(c.curve, c.last_temp);
                c.last_out = duty;
                // write pwm
                PwmDevice dev;
                dev.pwm_path = c.pwm_path;
                dev.enable_path = c.enable_path;
                Hwmon::setPwmPercent(dev, duty, nullptr);
            }
        }
        std::this_thread::sleep_for(1s);
    }
}
