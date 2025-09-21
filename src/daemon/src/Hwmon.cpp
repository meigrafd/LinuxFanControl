/*
 * Linux Fan Control — Hwmon (implementation)
 * (c) 2025 LinuxFanControl contributors
 */
#include "include/Hwmon.hpp"
#include "include/Utils.hpp"
#include "include/VendorMapping.hpp"
#include "include/Log.hpp"

#include <filesystem>
#include <algorithm>
#include <cctype>

namespace fs = std::filesystem;

namespace lfc {

HwmonInventory Hwmon::scan() {
    HwmonInventory s;
    if (!fs::exists("/sys/class/hwmon")) {
        LOG_WARN("Hwmon: /sys/class/hwmon not found");
        return s;
    }

    for (const auto& chip : fs::directory_iterator("/sys/class/hwmon")) {
        if (!chip.is_directory()) continue;
        const fs::path chipPath = chip.path();

        HwmonChip meta;
        meta.hwmonPath = chipPath.string();
        meta.name   = util::read_first_line(chipPath / "name");
        meta.vendor = Hwmon::chipVendorForName(meta.name);
        s.chips.push_back(meta);
        LOG_DEBUG("Hwmon: chip=%s vendor=%s", meta.name.c_str(), meta.vendor.c_str());

        // temps
        for (const auto& f : fs::directory_iterator(chipPath)) {
            const auto bn = f.path().filename().string();
            if (bn.rfind("temp", 0) == 0 && bn.find("_input") != std::string::npos) {
                const std::string base = bn.substr(0, bn.find('_'));
                const std::string lbl  = util::read_first_line(f.path().parent_path() / (base + "_label"));
                s.temps.push_back({chipPath.string(), f.path().string(), lbl});
            }
        }
        // fans
        for (const auto& f : fs::directory_iterator(chipPath)) {
            const auto bn = f.path().filename().string();
            if (bn.rfind("fan", 0) == 0 && bn.find("_input") != std::string::npos) {
                const std::string base = bn.substr(0, bn.find('_'));
                const std::string lbl  = util::read_first_line(f.path().parent_path() / (base + "_label"));
                s.fans.push_back({chipPath.string(), f.path().string(), lbl});
            }
        }
        // pwms
        for (const auto& f : fs::directory_iterator(chipPath)) {
            const auto bn = f.path().filename().string();
            if (bn.rfind("pwm", 0) == 0 && bn.find('_') == std::string::npos) {
                const fs::path pwm    = f.path();
                const fs::path enable = chipPath / (bn + "_enable");
                int pwm_max = 255;
                (void)util::read_int_file((chipPath / (bn + "_max")).string(), pwm_max);
                const std::string lbl = util::read_first_line(chipPath / (bn + "_label"));
                s.pwms.push_back({chipPath.string(), pwm.string(), enable.string(), pwm_max, lbl});
            }
        }
    }
    LOG_INFO("Hwmon: scan complete (chips=%zu temps=%zu fans=%zu pwms=%zu)",
             s.chips.size(), s.temps.size(), s.fans.size(), s.pwms.size());
    return s;
}

void Hwmon::refreshValues(HwmonInventory& s) {
    // NOTE: This does *not* discover new hw. It only purges disappeared nodes
    // and updates metadata like pwm_max if those files changed.
    auto existsFile = [](const std::string& p) {
        std::error_code ec;
        return fs::exists(p, ec);
    };

    // Temps
    {
        std::vector<HwmonTemp> out;
        out.reserve(s.temps.size());
        for (const auto& t : s.temps) {
            if (existsFile(t.path_input)) {
                out.push_back(t); // nothing to "refresh" here; values are read on demand
            } else {
                LOG_DEBUG("Hwmon: drop temp (gone): %s", t.path_input.c_str());
            }
        }
        s.temps.swap(out);
    }

    // Fans
    {
        std::vector<HwmonFan> out;
        out.reserve(s.fans.size());
        for (const auto& f : s.fans) {
            if (existsFile(f.path_input)) {
                out.push_back(f);
            } else {
                LOG_DEBUG("Hwmon: drop fan (gone): %s", f.path_input.c_str());
            }
        }
        s.fans.swap(out);
    }

    // PWMs
    {
        std::vector<HwmonPwm> out;
        out.reserve(s.pwms.size());
        for (auto p : s.pwms) {
            if (!existsFile(p.path_pwm) || !existsFile(p.path_enable)) {
                LOG_DEBUG("Hwmon: drop pwm (gone): %s", p.path_pwm.c_str());
                continue;
            }
            // Refresh pwm_max if file present (may differ across boots/drivers)
            try {
                fs::path base = fs::path(p.path_pwm).parent_path();
                fs::path chip = base.parent_path();
                int maxv = p.pwm_max;
                (void)util::read_int_file((chip / (fs::path(p.path_pwm).filename().string() + "_max")).string(), maxv);
                p.pwm_max = std::clamp(maxv, 1, 65535);
            } catch (...) {
                // keep previous pwm_max on any error
            }
            out.push_back(p);
        }
        s.pwms.swap(out);
    }

    // Chips: keep entries whose directory still exists; update vendor name if "name" changed
    {
        std::vector<HwmonChip> out;
        out.reserve(s.chips.size());
        for (auto c : s.chips) {
            if (!existsFile(c.hwmonPath)) {
                LOG_DEBUG("Hwmon: drop chip (gone): %s", c.hwmonPath.c_str());
                continue;
            }
            std::string newName = chipNameForPath(c.hwmonPath);
            if (!newName.empty() && newName != c.name) {
                c.name = std::move(newName);
                c.vendor = chipVendorForName(c.name);
            }
            out.push_back(c);
        }
            s.chips.swap(out);
    }

    LOG_TRACE("Hwmon: refreshValues done (chips=%zu temps=%zu fans=%zu pwms=%zu)",
              s.chips.size(), s.temps.size(), s.fans.size(), s.pwms.size());
}

std::optional<double> Hwmon::readTempC(const HwmonTemp& t) {
    int millideg = 0;
    if (!util::read_int_file(t.path_input, millideg)) {
        LOG_WARN("Hwmon: read temp failed: %s", t.path_input.c_str());
        return std::nullopt;
    }
    return (double)millideg / 1000.0;
}

std::optional<int> Hwmon::readRpm(const HwmonFan& f) {
    int rpm = 0;
    if (!util::read_int_file(f.path_input, rpm)) {
        LOG_WARN("Hwmon: read rpm failed: %s", f.path_input.c_str());
        return std::nullopt;
    }
    return rpm;
}

std::optional<int> Hwmon::readPercent(const HwmonPwm& p) {
    int raw = 0;
    if (!util::read_int_file(p.path_pwm, raw)) {
        LOG_WARN("Hwmon: read pwm failed: %s", p.path_pwm.c_str());
        return std::nullopt;
    }
    const int maxv = p.pwm_max > 0 ? p.pwm_max : 255;
    return util::pwmPercentFromRaw(raw, maxv);
}

std::optional<int> Hwmon::readRaw(const HwmonPwm& p) {
    int raw = 0;
    if (!util::read_int_file(p.path_pwm, raw)) {
        LOG_WARN("Hwmon: read raw failed: %s", p.path_pwm.c_str());
        return std::nullopt;
    }
    return raw;
}

std::optional<int> Hwmon::readEnable(const HwmonPwm& p) {
    int m = 0;
    if (!util::read_int_file(p.path_enable, m)) {
        LOG_WARN("Hwmon: read enable failed: %s", p.path_enable.c_str());
        return std::nullopt;
    }
    return m;
}

bool Hwmon::setPercent(const HwmonPwm& p, int percent) {
    const int maxv = p.pwm_max > 0 ? p.pwm_max : 255;
    const int pct = std::clamp(percent, 0, 100);
    const int raw = (pct * maxv + 50) / 100;
    const bool ok = util::write_int_file(p.path_pwm, raw);
    if (!ok) {
        LOG_WARN("Hwmon: setPercent failed: %s <- %d%% (raw=%d)", p.path_pwm.c_str(), pct, raw);
    } else {
        LOG_TRACE("Hwmon: setPercent: %s <- %d%% (raw=%d)", p.path_pwm.c_str(), pct, raw);
    }
    return ok;
}

bool Hwmon::setRaw(const HwmonPwm& p, int raw) {
    const int maxv = p.pwm_max > 0 ? p.pwm_max : 255;
    const int clamped = std::clamp(raw, 0, maxv);
    const bool ok = util::write_int_file(p.path_pwm, clamped);
    if (!ok) {
        LOG_WARN("Hwmon: setRaw failed: %s <- %d", p.path_pwm.c_str(), clamped);
    } else {
        LOG_TRACE("Hwmon: setRaw: %s <- %d", p.path_pwm.c_str(), clamped);
    }
    return ok;
}

bool Hwmon::setEnable(const HwmonPwm& p, int mode) {
    const bool ok = util::write_int_file(p.path_enable, mode);
    if (!ok) {
        LOG_WARN("Hwmon: setEnable failed: %s <- %d", p.path_enable.c_str(), mode);
    } else {
        LOG_TRACE("Hwmon: setEnable: %s <- %d", p.path_enable.c_str(), mode);
    }
    return ok;
}

bool Hwmon::writeRaw(const std::string& path, int raw) {
    const bool ok = util::write_int_file(path, raw);
    if (!ok) {
        LOG_WARN("Hwmon: writeRaw failed: %s <- %d", path.c_str(), raw);
    }
    return ok;
}

bool Hwmon::writeEnable(const std::string& path, int mode) {
    const bool ok = util::write_int_file(path, mode);
    if (!ok) {
        LOG_WARN("Hwmon: writeEnable failed: %s <- %d", path.c_str(), mode);
    }
    return ok;
}

std::string Hwmon::chipNameForPath(const std::string& chipPath) {
    return util::read_first_line(fs::path(chipPath) / "name");
}

std::string Hwmon::chipVendorForName(const std::string& chipName) {
    return VendorMapping::instance().vendorFor(chipName);
}

} // namespace lfc
