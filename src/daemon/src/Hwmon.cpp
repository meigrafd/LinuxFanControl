/*
 * Linux Fan Control — Hwmon sysfs snapshot (implementation)
 * - Normalizes all discovered sysfs paths to canonical/lexical form
 * - Provides simple read/write helpers
 * (c) 2025 LinuxFanControl contributors
 */
#include "Hwmon.hpp"

#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <vector>
#include <optional>
#include <cstdlib>
#include <algorithm>
#include <cctype>

namespace fs = std::filesystem;

namespace lfc {
namespace {

// Return canonical path if possible; otherwise lexically normalized.
// This removes ".." segments even if the target does not fully exist.
std::string normalize_sys_path(const std::string& in) {
    std::error_code ec;
    fs::path p(in);
    fs::path out;
    // Try weakly_canonical (resolves existing parts; tolerates missing leaf)
    out = fs::weakly_canonical(p, ec);
    if (!ec && !out.empty()) {
        return out.string();
    }
    // Fallback: lexical normalization (pure string-level collapse)
    return p.lexically_normal().string();
}

static std::optional<std::string> slurp(const fs::path& p) {
    std::ifstream f(p);
    if (!f) return std::nullopt;
    std::string s;
    std::getline(f, s, '\0');
    return s;
}

static bool write_text(const fs::path& p, const std::string& s) {
    std::ofstream f(p);
    if (!f) return false;
    f << s;
    return true;
}

// Parse int from file (trim whitespace). Returns nullopt on errors.
static std::optional<long> read_long(const fs::path& p) {
    auto s = slurp(p);
    if (!s) return std::nullopt;
    // trim
    auto& t = *s;
    t.erase(std::remove_if(t.begin(), t.end(), [](unsigned char c){ return std::isspace(c); }), t.end());
    if (t.empty()) return std::nullopt;
    try {
        size_t idx = 0;
        long v = std::stol(t, &idx, 10);
        if (idx != t.size()) return std::nullopt;
        return v;
    } catch (...) {
        return std::nullopt;
    }
}

} // anonymous

HwmonSnapshot Hwmon::scan(const std::string& root) {
    HwmonSnapshot snap;

    std::error_code ec;
    fs::path rootp(root);
    for (const auto& ent : fs::directory_iterator(rootp, ec)) {
        if (ec) break;
        if (!ent.is_directory()) continue;

        fs::path dir = ent.path();

        // Gather temp*_input (+ optional labels)
        for (int i = 1; i <= 32; ++i) {
            fs::path pin  = dir / ("temp" + std::to_string(i) + "_input");
            if (!fs::exists(pin)) continue;

            HwmonTemp t;
            t.path_input = normalize_sys_path(pin.string());

            fs::path plabel = dir / ("temp" + std::to_string(i) + "_label");
            if (fs::exists(plabel)) {
                auto s = slurp(plabel);
                if (s && !s->empty()) {
                    // trim trailing newline
                    if (!s->empty() && (s->back() == '\n' || s->back() == '\r')) s->pop_back();
                    t.label = *s;
                }
            }
            snap.temps.push_back(std::move(t));
        }

        // Gather fan*_input
        for (int i = 1; i <= 16; ++i) {
            fs::path pin = dir / ("fan" + std::to_string(i) + "_input");
            if (!fs::exists(pin)) continue;

            HwmonFan f;
            f.path_input = normalize_sys_path(pin.string());
            snap.fans.push_back(std::move(f));
        }

        // Gather pwm* (+ enable)
        for (int i = 1; i <= 16; ++i) {
            fs::path ppwm    = dir / ("pwm" + std::to_string(i));
            fs::path penable = dir / ("pwm" + std::to_string(i) + "_enable");
            if (!fs::exists(ppwm) || !fs::exists(penable)) continue;

            HwmonPwm p;
            p.path_pwm    = normalize_sys_path(ppwm.string());
            p.path_enable = normalize_sys_path(penable.string());
            snap.pwms.push_back(std::move(p));
        }
    }

    return snap;
}

// ---------------- reading helpers ----------------

std::optional<int> Hwmon::readMilliC(const HwmonTemp& t) {
    auto v = read_long(t.path_input);
    if (!v) return std::nullopt;
    // Heuristics: Many drivers already expose milli-C. Some expose plain C.
    long raw = *v;
    if (std::abs(raw) < 1000) {
        // values like "42" -> interpret as °C
        return static_cast<int>(raw * 1000);
    }
    return static_cast<int>(raw);
}

std::optional<double> Hwmon::readTempC(const HwmonTemp& t) {
    auto mc = readMilliC(t);
    if (!mc) return std::nullopt;
    return static_cast<double>(*mc) / 1000.0;
}

std::optional<int> Hwmon::readRpm(const HwmonFan& f) {
    auto v = read_long(f.path_input);
    if (!v) return std::nullopt;
    return static_cast<int>(*v);
}

std::optional<int> Hwmon::readPercent(const HwmonPwm& p) {
    auto v = read_long(p.path_pwm);
    if (!v) return std::nullopt;
    // Common: 0..255 range or 0..100. Try to map into 0..100.
    long raw = *v;
    if (raw < 0) raw = 0;
    if (raw > 255) raw = 255;
    int pct = static_cast<int>((raw * 100 + 127) / 255);
    if (pct > 100) pct = 100;
    return pct;
}

// ---------------- write helpers ----------------

void Hwmon::setManual(const HwmonPwm& p) {
    // 1 = manual on most drivers (5 = manual on some); try 1 first.
    (void)write_text(p.path_enable, "1\n");
}

void Hwmon::setAuto(const HwmonPwm& p) {
    // 2 = auto on most drivers
    (void)write_text(p.path_enable, "2\n");
}

void Hwmon::setPercent(const HwmonPwm& p, int pct) {
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
    // Map 0..100 to 0..255
    int v = static_cast<int>(pct * 255 / 100);
    (void)write_text(p.path_pwm, std::to_string(v) + "\n");
}

} // namespace lfc
