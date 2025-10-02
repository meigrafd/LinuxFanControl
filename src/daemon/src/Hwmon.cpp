/*
 * Linux Fan Control — hwmon scanner + I/O
 * (c) 2025 LinuxFanControl contributors
 *
 * Responsibilities:
 *  - Enumerate /sys/class/hwmon/hwmonX into HwmonInventory
 *  - Read temps/fans/PWM values
 *  - Write PWM (raw or percent) and switch enable mode
 *  - Never drop a PWM just because pwmN_enable is missing or unreadable
 */

#include "include/Hwmon.hpp"
#include "include/Utils.hpp"
#include "include/Log.hpp"
#include "include/VendorMapping.hpp"

#include <algorithm>
#include <cerrno>
#include <cinttypes>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace fs = std::filesystem;

namespace lfc {

/* ------------------------------ fs helpers -------------------------------- */

static inline bool fileExists(const fs::path& p) {
    std::error_code ec;
    return fs::exists(p, ec);
}
static std::string readFirstLine(const fs::path& p) {
    std::ifstream f(p);
    std::string s;
    if (f) std::getline(f, s);
    return s;
}
static std::optional<long> readLong(const fs::path& p) {
    std::ifstream f(p);
    if (!f) return std::nullopt;
    long v = 0;
    f >> v;
    if (!f) return std::nullopt;
    return v;
}
static std::optional<int> readInt(const fs::path& p) {
    auto v = readLong(p);
    if (!v) return std::nullopt;
    return static_cast<int>(*v);
}
static bool writeInt(const fs::path& p, int value) {
    std::ofstream f(p);
    if (!f) return false;
    f << value;
    return static_cast<bool>(f);
}

/* ------------------------------ helpers ----------------------------------- */

static std::string chipName(const fs::path& base) {
    // Prefer "name", else directory leaf
    std::string n = readFirstLine(base / "name");
    if (!n.empty()) return n;
    return base.filename().string();
}

static std::string joinAliases(const std::vector<std::string>& v) {
    std::string out;
    for (size_t i = 0; i < v.size(); ++i) {
        out += v[i];
        if (i + 1 < v.size()) out += ",";
    }
    return out;
}

static std::string chipVendorPretty(const std::string& chipName) {
    // Prefer pretty vendor mapping; fall back to chip name.
    std::string pretty = VendorMapping::instance().vendorFor(chipName);
    if (!pretty.empty()) return pretty;
    return chipName;
}

std::string Hwmon::chipNameForPath(const std::string& chipPath) {
    if (chipPath.empty()) return {};
    try {
        const fs::path base(chipPath);
        // Versuche die "name"-Datei
        const std::string n = util::read_first_line(base / "name");
        if (!n.empty()) return n;
        // Fallback: Verzeichnisname (z. B. "hwmon4")
        return base.filename().string();
    } catch (...) {
        return {};
    }
}

std::string Hwmon::chipVendorForName(const std::string& chipName) {
    if (chipName.empty()) return {};
    return VendorMapping::instance().vendorForChipName(chipName);
}

/* ------------------------------ scanners ---------------------------------- */

static void scanTemps(const fs::path& base, const std::string& chipPath, std::vector<HwmonTemp>& out) {
    for (int i = 1; i <= 20; ++i) {
        fs::path in  = base / ("temp" + std::to_string(i) + "_input");
        if (!fileExists(in)) continue;
        fs::path lbl = base / ("temp" + std::to_string(i) + "_label");
        HwmonTemp t{};
        t.chipPath   = chipPath;
        t.path_input = in.string();
        t.label      = fileExists(lbl) ? readFirstLine(lbl) : std::string();
        out.push_back(std::move(t));
    }
}

static void scanFans(const fs::path& base, const std::string& chipPath, std::vector<HwmonFan>& out) {
    for (int i = 1; i <= 10; ++i) {
        fs::path in  = base / ("fan" + std::to_string(i) + "_input");
        if (!fileExists(in)) continue;
        fs::path lbl = base / ("fan" + std::to_string(i) + "_label");
        HwmonFan f{};
        f.chipPath   = chipPath;
        f.path_input = in.string();
        f.label      = fileExists(lbl) ? readFirstLine(lbl) : std::string();
        out.push_back(std::move(f));
    }
}

static void scanPwms(const fs::path& base, const std::string& chipPath, std::vector<HwmonPwm>& out) {
    for (int i = 1; i <= 10; ++i) {
        fs::path p    = base / ("pwm" + std::to_string(i));
        if (!fileExists(p)) continue;

        fs::path pen  = base / ("pwm" + std::to_string(i) + "_enable");
        fs::path pmax = base / ("pwm" + std::to_string(i) + "_max");

        HwmonPwm w{};
        w.chipPath    = chipPath;
        w.path_pwm    = p.string();
        w.path_enable = fileExists(pen) ? pen.string() : std::string();
        w.pwm_max     = readInt(pmax).value_or(255);

        LOG_DEBUG("Hwmon: pwm found chip=%s path=%s enable=%s max=%d",
                  w.chipPath.c_str(), w.path_pwm.c_str(),
                  w.path_enable.empty() ? "<none>" : w.path_enable.c_str(),
                  w.pwm_max);

        out.push_back(std::move(w));
    }
}

/* --------------------------------- scan ----------------------------------- */

HwmonInventory Hwmon::scan() {
    HwmonInventory inv;
    const fs::path root = "/sys/class/hwmon";

    std::error_code ec;
    if (!fs::exists(root, ec)) {
        LOG_WARN("Hwmon: root missing: %s", root.string().c_str());
        return inv;
    }

    for (const auto& dir : fs::directory_iterator(root, ec)) {
        if (ec) break;
        fs::path base = dir.path(); // .../hwmonX
        if (!fs::is_directory(base)) continue;

        HwmonChip chip{};
        chip.hwmonPath = fs::canonical(base, ec).string();
        const std::string n = chipName(base);
        chip.name   = n;
        chip.vendor = chipVendorPretty(n);

        LOG_DEBUG("Hwmon: chip=%s vendor=%s", n.c_str(), chip.vendor.c_str());
        const auto aliases = VendorMapping::instance().chipAliasesFor(n);
        if (!aliases.empty()) {
            LOG_DEBUG("Hwmon: chip=%s aliases=[%s]", n.c_str(), joinAliases(aliases).c_str());
        }

        inv.chips.push_back(chip);

        const std::string chipPath = chip.hwmonPath; // for child entries
        scanTemps(base, chipPath, inv.temps);
        scanFans(base, chipPath, inv.fans);
        scanPwms(base, chipPath, inv.pwms);
    }

    LOG_INFO("Hwmon: scan complete (chips=%zu temps=%zu fans=%zu pwms=%zu)",
             inv.chips.size(), inv.temps.size(), inv.fans.size(), inv.pwms.size());

    return inv;
}

/* ----------------------------- refresh values ----------------------------- */

void Hwmon::refreshValues(HwmonInventory& inv) {
    // Currently a no-op: values are read on demand by the read* functions.
    (void)inv;
}

/* ------------------------------ read helpers ------------------------------ */

std::optional<double> Hwmon::readTempC(const HwmonTemp& t) {
    auto mv = readLong(t.path_input);
    if (!mv) return std::nullopt;
    // hwmon temps are millidegree Celsius
    return static_cast<double>(*mv) / 1000.0;
}

std::optional<int> Hwmon::readRpm(const HwmonFan& f) {
    return readInt(f.path_input);
}

std::optional<int> Hwmon::readEnable(const HwmonPwm& p) {
    if (p.path_enable.empty()) return std::nullopt; // unknown
    return readInt(p.path_enable);
}

std::optional<int> Hwmon::readRaw(const HwmonPwm& p) {
    return readInt(p.path_pwm);
}

/* ------------------------------ write helpers ----------------------------- */

bool Hwmon::setEnable(const HwmonPwm& p, int mode) {
// Common modes: 0=auto, 1=manual (some drivers: 2=manual)
if (p.path_enable.empty()) {
    // Some devices (notably many GPUs) do not expose an enable path.
    // Treat this as a no-op success to avoid noisy warnings; engine will handle mode gracefully.
    LOG_TRACE("Hwmon: setEnable noop (no enable path) for %s", p.path_pwm.c_str());
    return true;
}
const bool ok = writeInt(p.path_enable, mode);
if (!ok) {
    LOG_WARN("Hwmon: setEnable failed path=%s errno=%d", p.path_enable.c_str(), errno);
} else {
    LOG_DEBUG("Hwmon: setEnable path=%s mode=%d", p.path_enable.c_str(), mode);
}
return ok;

}

bool Hwmon::setRaw(const HwmonPwm& p, int raw) {
    const int vmax = std::max(1, p.pwm_max);
    const int v = std::clamp(raw, 0, vmax);
    const bool ok = writeInt(p.path_pwm, v);
    if (!ok) {
        LOG_WARN("Hwmon: setRaw failed path=%s errno=%d", p.path_pwm.c_str(), errno);
    } else {
        LOG_DEBUG("Hwmon: setRaw path=%s value=%d", p.path_pwm.c_str(), v);
    }
    return ok;
}

bool Hwmon::setPercent(const HwmonPwm& p, int percent) {
    const int pc = std::clamp(percent, 0, 100);
    const int vmax = std::max(1, p.pwm_max);
    const int v  = static_cast<int>(std::lround((pc / 100.0) * vmax));
    return setRaw(p, v);
}

} // namespace lfc
