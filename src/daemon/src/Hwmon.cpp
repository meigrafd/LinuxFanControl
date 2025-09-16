/*
 * Linux Fan Control — Hwmon backends (implementation)
 * - Sysfs scanner with path normalization
 * - Optional libsensors reader (guarded by HAVE_LIBSENSORS)
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

#ifdef HAVE_LIBSENSORS
#include <sensors/sensors.h>
#include <cstring>
#endif

namespace fs = std::filesystem;

namespace lfc {
namespace {

// ---- common helpers --------------------------------------------------------

std::string normalize_sys_path(const std::string& in) {
    std::error_code ec;
    fs::path p(in);
    fs::path out = fs::weakly_canonical(p, ec);
    if (!ec && !out.empty()) return out.string();
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

static std::optional<long> read_long(const fs::path& p) {
    auto s = slurp(p);
    if (!s) return std::nullopt;
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

static bool is_libsensors_pseudo(const std::string& pathish) {
    return pathish.rfind("libsensors:", 0) == 0;
}

// ---- sysfs backend ---------------------------------------------------------

static HwmonSnapshot scan_sysfs(const std::string& root) {
    HwmonSnapshot snap;
    snap.backend = HwBackend::Sysfs;

    std::error_code ec;
    fs::path rootp(root);
    for (const auto& ent : fs::directory_iterator(rootp, ec)) {
        if (ec) break;
        if (!ent.is_directory()) continue;

        fs::path dir = ent.path();

        // temps
        for (int i = 1; i <= 64; ++i) {
            fs::path pin  = dir / ("temp" + std::to_string(i) + "_input");
            if (!fs::exists(pin)) continue;
            HwmonTemp t;
            t.path_input = normalize_sys_path(pin.string());
            fs::path pl = dir / ("temp" + std::to_string(i) + "_label");
            if (fs::exists(pl)) {
                if (auto s = slurp(pl)) {
                    while (!s->empty() && (s->back() == '\n' || s->back() == '\r')) s->pop_back();
                    t.label = *s;
                }
            }
            snap.temps.push_back(std::move(t));
        }

        // fans
        for (int i = 1; i <= 32; ++i) {
            fs::path pin = dir / ("fan" + std::to_string(i) + "_input");
            if (!fs::exists(pin)) continue;
            HwmonFan f;
            f.path_input = normalize_sys_path(pin.string());
            snap.fans.push_back(std::move(f));
        }

        // pwms
        for (int i = 1; i <= 32; ++i) {
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

// ---- libsensors backend ----------------------------------------------------

#ifdef HAVE_LIBSENSORS
static std::string chip_name_to_string(const sensors_chip_name* chip) {
    char buf[256];
    if (sensors_snprintf_chip_name(buf, sizeof(buf), chip) < 0) return "unknown";
    return std::string(buf);
}

static HwmonSnapshot scan_libsensors() {
    HwmonSnapshot snap;
    snap.backend = HwBackend::Libsensors;

    if (sensors_init(nullptr) != 0) {
        return snap;
    }

    int c = 0;
    const sensors_chip_name* chip;
    while ((chip = sensors_get_detected_chips(nullptr, &c)) != nullptr) {
        std::string chipName = chip_name_to_string(chip);

        int f = 0;
        const sensors_feature* feat;
        while ((feat = sensors_get_features(chip, &f)) != nullptr) {
            sensors_feature_type t = static_cast<sensors_feature_type>(feat->type);
            if (t != SENSORS_FEATURE_TEMP && t != SENSORS_FEATURE_FAN) continue;

            int sidx = 0;
            const sensors_subfeature* sf;
            while ((sf = sensors_get_all_subfeatures(chip, feat, &sidx)) != nullptr) {
                if (!sf) continue;

                // temp input or fan input
                if ((t == SENSORS_FEATURE_TEMP && sf->type == SENSORS_SUBFEATURE_TEMP_INPUT) ||
                    (t == SENSORS_FEATURE_FAN  && sf->type == SENSORS_SUBFEATURE_FAN_INPUT)) {

                    double val = 0.0;
                    bool have = sensors_get_value(chip, sf->number, &val) == 0;

                    std::string pseudo = "libsensors:" + chipName + ":" + sf->name;

                    if (t == SENSORS_FEATURE_TEMP) {
                        HwmonTemp tt;
                        tt.path_input = pseudo;
                        // prefer libsensors label if available
                        const char* flabel = sensors_get_label(chip, feat);
                        tt.label = (flabel ? std::string(flabel) : std::string());
                        snap.temps.push_back(std::move(tt));
                    } else {
                        HwmonFan ff;
                        ff.path_input = pseudo;
                        snap.fans.push_back(std::move(ff));
                    }
                }
            }
        }
    }

    sensors_cleanup();
    return snap;
}
#endif // HAVE_LIBSENSORS

} // anonymous namespace

// ----------------------------------------------------------------------------

HwmonSnapshot Hwmon::scan(HwBackend backend, const std::string& root) {
    // Prefer sysfs for control capability
    if (backend == HwBackend::Sysfs) {
        return scan_sysfs(root);
    }
#ifdef HAVE_LIBSENSORS
    if (backend == HwBackend::Libsensors) {
        return scan_libsensors();
    }
#endif
    // Auto: try sysfs first
    HwmonSnapshot s = scan_sysfs(root);
    if (!s.temps.empty() || !s.fans.empty() || !s.pwms.empty()) {
        s.backend = HwBackend::Sysfs;
        return s;
    }
#ifdef HAVE_LIBSENSORS
    HwmonSnapshot l = scan_libsensors();
    if (!l.temps.empty() || !l.fans.empty()) {
        l.backend = HwBackend::Libsensors;
        return l;
    }
#endif
    s.backend = HwBackend::Sysfs;
    return s; // empty snapshot
}

// ---------------- reading helpers ----------------

std::optional<int> Hwmon::readMilliC(const HwmonTemp& t) {
    if (is_libsensors_pseudo(t.path_input)) {
#ifdef HAVE_LIBSENSORS
        // Parse "libsensors:<chip>:<name>"
        const std::string s = t.path_input.substr(std::string("libsensors:").size());
        // Re-init for single read (cheap enough; avoids global handles in this layer)
        if (sensors_init(nullptr) != 0) return std::nullopt;

        int c = 0;
        const sensors_chip_name* chip;
        while ((chip = sensors_get_detected_chips(nullptr, &c)) != nullptr) {
            char buf[256];
            sensors_snprintf_chip_name(buf, sizeof(buf), chip);
            if (s.rfind(buf, 0) != 0) continue; // name prefix mismatch
            // find subfeature by name suffix
            int f = 0;
            const sensors_feature* feat;
            while ((feat = sensors_get_features(chip, &f)) != nullptr) {
                if (feat->type != SENSORS_FEATURE_TEMP) continue;
                int sidx = 0;
                const sensors_subfeature* sf;
                while ((sf = sensors_get_all_subfeatures(chip, feat, &sidx)) != nullptr) {
                    if (sf->type != SENSORS_SUBFEATURE_TEMP_INPUT) continue;
                    // match by subfeature name
                    if (s.find(sf->name) == std::string::npos) continue;
                    double val = 0.0;
                    if (sensors_get_value(chip, sf->number, &val) == 0) {
                        sensors_cleanup();
                        // libsensors returns in °C
                        return static_cast<int>(val * 1000.0);
                    }
                }
            }
        }
        sensors_cleanup();
        return std::nullopt;
#else
        return std::nullopt;
#endif
    }

    auto v = read_long(t.path_input);
    if (!v) return std::nullopt;
    long raw = *v;
    if (std::abs(raw) < 1000) return static_cast<int>(raw * 1000);
    return static_cast<int>(raw);
}

std::optional<double> Hwmon::readTempC(const HwmonTemp& t) {
    auto mc = readMilliC(t);
    if (!mc) return std::nullopt;
    return static_cast<double>(*mc) / 1000.0;
}

std::optional<int> Hwmon::readRpm(const HwmonFan& f) {
    if (is_libsensors_pseudo(f.path_input)) {
#ifdef HAVE_LIBSENSORS
        const std::string s = f.path_input.substr(std::string("libsensors:").size());
        if (sensors_init(nullptr) != 0) return std::nullopt;
        int c = 0;
        const sensors_chip_name* chip;
        while ((chip = sensors_get_detected_chips(nullptr, &c)) != nullptr) {
            char buf[256];
            sensors_snprintf_chip_name(buf, sizeof(buf), chip);
            if (s.rfind(buf, 0) != 0) continue;
            int fidx = 0;
            const sensors_feature* feat;
            while ((feat = sensors_get_features(chip, &fidx)) != nullptr) {
                if (feat->type != SENSORS_FEATURE_FAN) continue;
                int sidx = 0;
                const sensors_subfeature* sf;
                while ((sf = sensors_get_all_subfeatures(chip, feat, &sidx)) != nullptr) {
                    if (sf->type != SENSORS_SUBFEATURE_FAN_INPUT) continue;
                    if (s.find(sf->name) == std::string::npos) continue;
                    double val = 0.0;
                    if (sensors_get_value(chip, sf->number, &val) == 0) {
                        sensors_cleanup();
                        return static_cast<int>(val);
                    }
                }
            }
        }
        sensors_cleanup();
        return std::nullopt;
#else
        return std::nullopt;
#endif
    }

    auto v = read_long(f.path_input);
    if (!v) return std::nullopt;
    return static_cast<int>(*v);
}

std::optional<int> Hwmon::readPercent(const HwmonPwm& p) {
    auto v = read_long(p.path_pwm);
    if (!v) return std::nullopt;
    long raw = *v;
    if (raw < 0) raw = 0;
    if (raw > 255) raw = 255;
    int pct = static_cast<int>((raw * 100 + 127) / 255);
    if (pct > 100) pct = 100;
    return pct;
}

// ---------------- write helpers (sysfs only) ----------------

void Hwmon::setManual(const HwmonPwm& p) {
    (void)write_text(p.path_enable, "1\n");
}

void Hwmon::setAuto(const HwmonPwm& p) {
    (void)write_text(p.path_enable, "2\n");
}

void Hwmon::setPercent(const HwmonPwm& p, int pct) {
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
    int v = static_cast<int>(pct * 255 / 100);
    (void)write_text(p.path_pwm, std::to_string(v) + "\n");
}

} // namespace lfc
