/*
 * Linux Fan Control (LFC) - hwmon helpers
 * (c) 2025 meigrafd & contributors - MIT License (see LICENSE)
 */

#include "Hwmon.h"

#include <dirent.h>
#include <sys/stat.h>
#include <cerrno>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>
#include <cmath>
#include <cctype>
#include <limits>

// -------------------------
// small string helpers
// -------------------------
static bool ends_with(const std::string& s, const char* suff) {
    const size_t ls = s.size();
    const size_t lt = std::char_traits<char>::length(suff);
    return (ls >= lt) && (s.compare(ls - lt, lt, suff) == 0);
}

static bool starts_with(const std::string& s, const char* pref) {
    const size_t lp = std::char_traits<char>::length(pref);
    return s.size() >= lp && s.compare(0, lp, pref) == 0;
}

static std::string read_all(const std::string& p) {
    std::ifstream f(p);
    if (!f.good()) return {};
    std::string s;
    std::getline(f, s);
    // trim CR/LF
    s.erase(std::remove_if(s.begin(), s.end(),
                           [](unsigned char c){ return c=='\r' || c=='\n'; }), s.end());
    return s;
}

static bool write_all(const std::string& p, const std::string& text, std::string* err) {
    std::ofstream f(p);
    if (!f.good()) {
        if (err) *err = std::string("open failed: ") + std::strerror(errno);
        return false;
    }
    f << text;
    if (!f.good()) {
        if (err) *err = std::string("write failed: ") + std::strerror(errno);
        return false;
    }
    return true;
}

static double clamp01(double v) { return std::max(0.0, std::min(1.0, v)); }

// -------------------------
// temperatures
// -------------------------
std::vector<TempSensorInfo> Hwmon::discoverTemps() {
    std::vector<TempSensorInfo> out;
    DIR* d = ::opendir("/sys/class/hwmon");
    if (!d) return out;

    while (auto* e = ::readdir(d)) {
        if (!e || e->d_name[0]=='.') continue;
        const std::string base = std::string("/sys/class/hwmon/") + e->d_name;
        const std::string name = read_all(base + "/name");

        // collect labels
        std::vector<std::pair<std::string,std::string>> labels;
        if (DIR* d2 = ::opendir(base.c_str())) {
            while (auto* e2 = ::readdir(d2)) {
                if (!e2) continue;
                std::string fn = e2->d_name;
                if (starts_with(fn, "temp") && ends_with(fn, "_label") && fn.size()>6) {
                    const std::string key = fn.substr(0, fn.size()-6);
                    const std::string lab = read_all(base + "/" + fn);
                    labels.emplace_back(key, lab);
                }
            }
            ::closedir(d2);
        }

        // collect inputs
        if (DIR* d2 = ::opendir(base.c_str())) {
            while (auto* e2 = ::readdir(d2)) {
                if (!e2) continue;
                std::string fn = e2->d_name;
                if (starts_with(fn, "temp") && ends_with(fn, "_input") && fn.size()>6) {
                    const std::string key = fn.substr(0, fn.size()-6);
                    std::string lab = key;
                    auto it = std::find_if(labels.begin(), labels.end(),
                                           [&](const auto& p){ return p.first==key; });
                    if (it!=labels.end()) lab = it->second;

                    TempSensorInfo info;
                    // NOTE: TempSensorInfo provides name/label/path.
                    info.name  = name;
                    info.label = lab;
                    info.path  = base + "/" + fn;
                    out.push_back(std::move(info));
                }
            }
            ::closedir(d2);
        }
    }

    ::closedir(d);
    return out;
}

double Hwmon::readTempC(const std::string& path) {
    std::ifstream f(path);
    if (!f.good()) return std::numeric_limits<double>::quiet_NaN();

    std::string s; std::getline(f, s);
    if (s.empty()) return std::numeric_limits<double>::quiet_NaN();

    char* endp = nullptr;
    errno = 0;
    const long long raw = std::strtoll(s.c_str(), &endp, 10);
    if (errno!=0 || endp==s.c_str()) return std::numeric_limits<double>::quiet_NaN();

    // hwmon often uses millidegree
    if (raw > 200) return static_cast<double>(raw) / 1000.0;
    return static_cast<double>(raw);
}

// -------------------------
// PWM discovery
// -------------------------
std::vector<PwmDevice> Hwmon::discoverPwms() {
    std::vector<PwmDevice> out;

    DIR* d = ::opendir("/sys/class/hwmon");
    if (!d) return out;

    while (auto* e = ::readdir(d)) {
        if (!e || e->d_name[0]=='.') continue;
        const std::string hw = e->d_name;
        const std::string base = std::string("/sys/class/hwmon/") + hw;
        const std::string devName = read_all(base + "/name");

        // index all entries once
        std::vector<std::string> entries;
        if (DIR* d2 = ::opendir(base.c_str())) {
            while (auto* e2 = ::readdir(d2)) {
                if (!e2 || e2->d_name[0]=='.') continue;
                entries.emplace_back(e2->d_name);
            }
            ::closedir(d2);
        }

        // find pwmN files
        for (const auto& fn : entries) {
            if (!starts_with(fn, "pwm")) continue;
            if (ends_with(fn.c_str(), "_enable")) continue;

            // pwm number
            std::string num;
            for (size_t i=3;i<fn.size() && std::isdigit(static_cast<unsigned char>(fn[i]));++i)
                num.push_back(fn[i]);
            if (num.empty()) continue;

            const std::string pwmPath    = base + "/" + fn;
            const std::string enablePath = base + "/pwm" + num + "_enable";
            std::string tachPath;

            // try fanN_input with matching index (most common)
            const std::string fanCandidate = base + "/fan" + num + "_input";
            struct stat st{};
            if (::stat(fanCandidate.c_str(), &st)==0 && S_ISREG(st.st_mode)) {
                tachPath = fanCandidate;
            } else {
                // fallback: first fan*_input if any
                for (const auto& tf : entries) {
                    if (starts_with(tf, "fan") && ends_with(tf, "_input")) {
                        tachPath = base + "/" + tf;
                        break;
                    }
                }
            }

            PwmDevice pd;
            // IMPORTANT: field names chosen to match our project mappings (snake_case).
            // If your Hwmon.h uses other names, adapt here accordingly.
            pd.id             = hw + ":" + devName + ":" + fn;
            pd.pwm_path       = pwmPath;
            pd.enable_path    = enablePath;
            pd.fan_input_path = tachPath;

            out.push_back(std::move(pd));
        }
    }

    ::closedir(d);
    return out;
}

// -------------------------
// PWM write helper
// -------------------------
bool Hwmon::setPwmPercent(const PwmDevice& dev, double percent, std::string* err) {
    // clamp 0..100 and convert to 0..255
    const int duty255 = static_cast<int>(std::lround(clamp01(percent/100.0) * 255.0));

    // Best-effort: set manual (1) if enable file exists
    if (!dev.enable_path.empty()) {
        struct stat st{};
        if (::stat(dev.enable_path.c_str(), &st)==0 && S_ISREG(st.st_mode)) {
            std::string eerr;
            if (!write_all(dev.enable_path, "1", &eerr)) {
                if (err) *err = std::string("pwm_enable write failed: ") + eerr;
                // continue anyway; some drivers reject this while still allowing pwm writes
            }
        }
    }

    // Write pwm value
    std::ofstream f(dev.pwm_path);
    if (!f.good()) {
        if (err) *err = std::string("open pwm failed: ") + std::strerror(errno);
        return false;
    }
    f << duty255;
    if (!f.good()) {
        if (err) *err = std::string("pwm write failed: ") + std::strerror(errno);
        return false;
    }
    return true;
}
