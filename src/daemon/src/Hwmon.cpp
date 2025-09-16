/*
 * Linux Fan Control — Hwmon interface (implementation)
 * Thin sysfs helpers for reading sensors and controlling PWM
 * (c) 2025 LinuxFanControl contributors
 */
#include "Hwmon.hpp"

#include <fstream>
#include <sstream>
#include <iomanip>
#include <filesystem>
#include <algorithm>
#include <cctype>

namespace fs = std::filesystem;

namespace lfc {

static std::optional<long long> read_ll(const std::string& path) {
    try {
        std::ifstream f(path);
        if (!f) return std::nullopt;
        long long v = 0;
        f >> v;
        return v;
    } catch (...) { return std::nullopt; }
}

static bool write_int(const std::string& path, int v) {
    try {
        std::ofstream f(path);
        if (!f) return false;
        f << v;
        return true;
    } catch (...) { return false; }
}

HwmonInventory Hwmon::scan() {
    HwmonInventory inv;

    for (const auto& dir : fs::directory_iterator("/sys/class/hwmon")) {
        fs::path base = dir.path();
        if (!fs::is_directory(base)) continue;

        // temps
        for (int i = 1; i <= 16; ++i) {
            fs::path p_in = base / ("temp" + std::to_string(i) + "_input");
            if (!fs::exists(p_in)) continue;

            HwmonTemp t;
            t.path_input = p_in.string();

            fs::path p_lbl = base / ("temp" + std::to_string(i) + "_label");
            if (fs::exists(p_lbl)) {
                std::ifstream f(p_lbl);
                std::string lbl;
                std::getline(f, lbl);
                while (!lbl.empty() && std::isspace(static_cast<unsigned char>(lbl.back()))) lbl.pop_back();
                t.label = lbl;
            }

            inv.temps.push_back(std::move(t));
        }

        // fans
        for (int i = 1; i <= 16; ++i) {
            fs::path p_in = base / ("fan" + std::to_string(i) + "_input");
            if (!fs::exists(p_in)) continue;

            HwmonFan f;
            f.path_input = p_in.string();
            inv.fans.push_back(std::move(f));
        }

        // pwms
        for (int i = 1; i <= 16; ++i) {
            fs::path p_pwm = base / ("pwm" + std::to_string(i));
            if (!fs::exists(p_pwm)) continue;

            HwmonPwm p;
            p.path_pwm    = p_pwm.string();
            p.path_enable = (base / ("pwm" + std::to_string(i) + "_enable")).string();
            inv.pwms.push_back(std::move(p));
        }
    }

    return inv;
}

std::optional<double> Hwmon::readTempC(const HwmonTemp& t) {
    auto raw = read_ll(t.path_input);
    if (!raw.has_value()) return std::nullopt;
    // temp*_input is usually millidegree C
    return static_cast<double>(*raw) / 1000.0;
}

std::optional<int> Hwmon::readRpm(const HwmonFan& f) {
    auto v = read_ll(f.path_input);
    if (!v.has_value()) return std::nullopt;
    return static_cast<int>(*v);
}

// Best-effort read of percent; if pwm value is 0..255 scale to 0..100
std::optional<int> Hwmon::readPercent(const HwmonPwm& p) {
    auto raw = read_ll(p.path_pwm);
    if (!raw.has_value()) return std::nullopt;

    long long v = *raw;
    if (v < 0) v = 0;
    if (v > 255) v = 255;
    int percent = static_cast<int>((v * 100 + 127) / 255);
    return std::max(0, std::min(100, percent));
}

std::optional<int> Hwmon::readRaw(const HwmonPwm& p) {
    auto raw = read_ll(p.path_pwm);
    if (!raw.has_value()) return std::nullopt;
    long long v = *raw;
    if (v < 0) v = 0;
    if (v > 255) v = 255;
    return static_cast<int>(v);
}

void Hwmon::setPercent(const HwmonPwm& p, int percent) {
    // Ensure manual mode
    (void)write_int(p.path_enable, 1);

    int v = std::max(0, std::min(100, percent));
    // map 0..100 to 0..255
    int raw = (v * 255 + 50) / 100;
    (void)write_int(p.path_pwm, raw);
}

void Hwmon::writeRawPath(const std::string& path_pwm, int raw) {
    if (raw < 0) raw = 0;
    if (raw > 255) raw = 255;
    (void)write_int(path_pwm, raw);
}

std::optional<int> Hwmon::readEnable(const HwmonPwm& p) {
    auto v = read_ll(p.path_enable);
    if (!v.has_value()) return std::nullopt;
    return static_cast<int>(*v);
}

void Hwmon::writeEnable(const std::string& path_enable, int mode) {
    (void)write_int(path_enable, mode);
}

void Hwmon::writeEnable(const HwmonPwm& p, int mode) {
    (void)write_int(p.path_enable, mode);
}

std::string Hwmon::percentToString(int v) {
    v = std::max(0, std::min(100, v));
    std::ostringstream oss;
    oss << v << "%";
    return oss.str();
}

} // namespace lfc
