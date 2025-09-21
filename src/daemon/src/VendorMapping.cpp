/*
 * Linux Fan Control — Vendor mapping (runtime, hot-reload)
 * (c) 2025 LinuxFanControl contributors
 */
#include "include/VendorMapping.hpp"
#include "include/Utils.hpp"
#include "include/Log.hpp"

#include <nlohmann/json.hpp>
#include <fstream>
#include <climits>
#include <regex>

#ifdef __linux__
#  include <sys/inotify.h>
#  include <unistd.h>
#  include <fcntl.h>
#  include <errno.h>
#endif

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace lfc {

/* ============================== internal rule ============================== */

struct VendorMapping::Rule {
    std::regex   rx;
    std::string  vendor;
    int          priority{0};
    std::string  raw;
};

/* ---------------------- regex helper: flags & (?i) ------------------------- */

static bool compile_regex_from_json(const json& it, std::regex& out, std::string& raw, std::string& err) {
    err.clear();
    if (!it.contains("regex") || !it.at("regex").is_string()) {
        err = "missing 'regex' string";
        return false;
    }
    std::string pattern = it.at("regex").get<std::string>();
    raw = pattern;

    // Flags from JSON, e.g. {"flags":"i"} or {"flags":"is"}
    bool icase = false;
    if (it.contains("flags") && it.at("flags").is_string()) {
        std::string flags = it.at("flags").get<std::string>();
        for (char c : flags) {
            if (c == 'i' || c == 'I') icase = true;
        }
    }

    // Inline (?i) at pattern start (ECMAScript unterstützt das NICHT) -> strip & set icase
    if (pattern.size() >= 4 && pattern.rfind("(?i)", 0) == 0) {
        icase = true;
        pattern.erase(0, 4);
    }
    // auch die Variante "(?i:...)" am Anfang tolerieren → strip nur das Präfix "(?i:"
    if (pattern.size() >= 5 && pattern.rfind("(?i:", 0) == 0) {
        icase = true;
        pattern.erase(0, 4); // behalte ':' im Pattern (ECMAScript kennt das zwar nicht; daher vermeiden wir diese Form am besten in JSON)
        // Hinweis: Für maximale Kompatibilität sollten Nutzer "(?i:...)" nicht verwenden.
    }

    auto syntax = std::regex::ECMAScript;
    auto flags  = icase ? (syntax | std::regex::icase) : syntax;

    try {
        out = std::regex(pattern, flags);
        return true;
    } catch (const std::exception& ex) {
        err = ex.what();
        return false;
    }
}

/* ============================== lifecycle ================================== */

VendorMapping& VendorMapping::instance() {
    static VendorMapping inst;
    return inst;
}

VendorMapping::~VendorMapping() {
    std::unique_lock lk(mtx_);
    shutdownInotifyUnlocked();
}

/* ============================== config ===================================== */

void VendorMapping::setOverridePath(const std::string& path) {
    std::unique_lock lk(mtx_);
    overridePath_ = path;
    rules_.clear();
    activePath_.clear();
    activeMtime_.reset();
    lastCheck_ = {};
    shutdownInotifyUnlocked();
}

void VendorMapping::setWatchMode(WatchMode mode, int throttleMs) {
    std::unique_lock lk(mtx_);
    mode_ = mode;
    throttleMs_ = (throttleMs < 0 ? 0 : throttleMs);

    if (mode_ == WatchMode::Inotify && !activePath_.empty()) {
        shutdownInotifyUnlocked();
        initInotifyUnlocked();
    } else if (mode_ == WatchMode::MTime) {
        shutdownInotifyUnlocked();
    }
}

/* ============================== lookup ===================================== */

std::string VendorMapping::vendorForChipName(const std::string& chipName) {
    if (chipName.empty()) return std::string();

    ensureLoaded();
    tryReloadIfChanged();

    {
        std::shared_lock lk(mtx_);
        int bestPri = INT_MIN;
        const Rule* best = nullptr;
        for (const auto& r : rules_) {
            if (std::regex_search(chipName, r.rx)) {
                if (r.priority > bestPri) {
                    bestPri = r.priority;
                    best = &r;
                }
            }
        }
        if (best) return best->vendor;
    }
    return fallbackVendor(chipName);
}

/* ============================== loading ==================================== */
void VendorMapping::ensureLoaded() {
    std::shared_lock readlk(mtx_);
    if (!rules_.empty() || !activePath_.empty()) return;
    readlk.unlock();

    std::unique_lock lk(mtx_);
    if (!rules_.empty() || !activePath_.empty()) return; // double-checked

    std::string err;

    if (!overridePath_.empty()) {
        fs::path ov = fs::path(util::expandUserPath(overridePath_));
        if (loadFromFile(ov, err)) {
            activePath_  = ov;
            activeMtime_ = safe_mtime(ov);
            LOG_INFO("vendor-map: loaded (override) %s (rules=%zu)", ov.string().c_str(), rules_.size());
            if (mode_ == WatchMode::Inotify) initInotifyUnlocked();
            return;
        } else {
            LOG_WARN("vendor-map: override path failed: %s (%s)", ov.string().c_str(), err.c_str());
            return;
        }
    }

    auto paths = defaultSearchPaths();
    std::string tried;
    for (size_t i = 0; i < paths.size(); ++i) {
        if (i) tried += ", ";
        tried += paths[i].string();
    }
    LOG_DEBUG("vendor-map: probing default paths: %s", tried.c_str());

    for (const auto& p : paths) {
        if (loadFromFile(p, err)) {
            activePath_  = p;
            activeMtime_ = safe_mtime(p);
            LOG_INFO("vendor-map: loaded %s (rules=%zu)", p.string().c_str(), rules_.size());
            if (mode_ == WatchMode::Inotify) initInotifyUnlocked();
            return;
        } else {
            LOG_DEBUG("vendor-map: not here: %s (%s)", p.string().c_str(), err.c_str());
        }
    }

    LOG_DEBUG("vendor-map: no mapping file found in defaults; using fallback heuristics");
    // not fatal — fallback heuristics will be used
}

void VendorMapping::tryReloadIfChanged() {
    std::unique_lock lk(mtx_);

    if (activePath_.empty()) return;

    if (mode_ == WatchMode::Inotify) {
        if (!inotifyArmed_) {
            initInotifyUnlocked(); // lazy arm
        }
        bool changed = inotifyDrainEventsUnlocked();
        if (!changed) return;
        // fallthrough: proceed to reload
    } else {
        const auto now = std::chrono::steady_clock::now();
        if (lastCheck_.time_since_epoch().count() != 0 &&
            now - lastCheck_ < std::chrono::milliseconds(throttleMs_)) {
            return;
        }
        lastCheck_ = now;

        auto mt = safe_mtime(activePath_);
        if (!mt.has_value() || (activeMtime_.has_value() && *mt == *activeMtime_)) {
            return; // unchanged
        }
        activeMtime_ = mt; // record seen mtime
    }

    std::ifstream f(activePath_);
    if (!f) {
        LOG_WARN("vendor-map: cannot reopen %s", activePath_.string().c_str());
        return;
    }
    json j;
    try { f >> j; }
    catch (const std::exception& ex) {
        LOG_WARN("vendor-map: invalid json on reload: %s", ex.what());
        return;
    }
    if (!j.is_array()) {
        LOG_WARN("vendor-map: json must be an array");
        return;
    }

    std::vector<Rule> newRules;
    for (size_t i = 0; i < j.size(); ++i) {
        const auto& it = j[i];
        if (!it.contains("vendor")) continue;

        std::regex compiled;
        std::string raw;
        std::string rerr;
        if (!compile_regex_from_json(it, compiled, raw, rerr)) {
            LOG_WARN("vendor-map: bad regex at index %zu: %s", i, rerr.c_str());
            continue;
        }

        const std::string vendor = it.at("vendor").get<std::string>();
        const int pri = it.value("priority", 0);
        newRules.push_back(Rule{compiled, vendor, pri, raw});
    }
    if (!newRules.empty()) {
        rules_.swap(newRules);
        activeMtime_ = safe_mtime(activePath_);
        LOG_INFO("vendor-map: reloaded %s (rules=%zu)", activePath_.string().c_str(), rules_.size());
    }
}

/* ============================== file I/O =================================== */

bool VendorMapping::loadFromFile(const fs::path& p, std::string& err) {
    std::error_code ec;
    if (!fs::exists(p, ec) || ec) {
        err = "file not found: " + p.string();
        return false;
    }
    std::ifstream f(p);
    if (!f) { err = "cannot open: " + p.string(); return false; }

    json j;
    try { f >> j; }
    catch (const std::exception& ex) {
        err = std::string("invalid json: ") + ex.what();
        return false;
    }
    if (!j.is_array()) {
        err = "json must be an array of rules";
        return false;
    }

    std::vector<Rule> newRules;
    newRules.reserve(j.size());
    for (size_t i = 0; i < j.size(); ++i) {
        const auto& it = j[i];
        if (!it.contains("vendor")) continue;

        std::regex compiled;
        std::string raw;
        std::string rerr;
        if (!compile_regex_from_json(it, compiled, raw, rerr)) {
            LOG_WARN("vendor-map: bad regex at index %zu: %s", i, rerr.c_str());
            continue;
        }

        const std::string vendor = it.at("vendor").get<std::string>();
        const int pri = it.value("priority", 0);
        newRules.push_back(Rule{compiled, vendor, pri, raw});
    }

    if (newRules.empty()) {
        err = "no usable rules in " + p.string();
        return false;
    }
    rules_.swap(newRules);
    return true;
}

/* ============================== paths & misc =============================== */

std::vector<fs::path> VendorMapping::defaultSearchPaths() const {
    std::vector<fs::path> v;
    if (const char* env = std::getenv("LFC_VENDOR_MAP")) {
        if (*env) v.emplace_back(util::expandUserPath(env));
    }
    v.emplace_back(util::expandUserPath("~/.config/LinuxFanControl/vendorMapping.json"));
    v.emplace_back("/etc/LinuxFanControl/vendorMapping.json");
    v.emplace_back("/usr/share/LinuxFanControl/vendorMapping.json");
    return v;
}

std::optional<fs::file_time_type> VendorMapping::safe_mtime(const fs::path& p) const {
    std::error_code ec;
    auto mt = fs::last_write_time(p, ec);
    if (ec) return std::nullopt;
    return mt;
}

std::string VendorMapping::fallbackVendor(const std::string& chipName) {
    const std::string s = util::to_lower(chipName);

    if (s.find("amdgpu") != std::string::npos) return "AMD";
    if (s.find("nvidia") != std::string::npos) return "NVIDIA";
    if (s.find("i915")   != std::string::npos || s.find("intel") != std::string::npos) return "Intel";
    if (s.find("acpitz")   != std::string::npos) return "ACPI";
    if (s.find("k10temp")  != std::string::npos) return "AMD";
    if (s.find("coretemp") != std::string::npos) return "Intel";
    return {};
}

/* ============================== inotify ==================================== */

void VendorMapping::initInotifyUnlocked() {
#ifdef __linux__
    if (inotifyFd_ < 0 && !activePath_.empty()) {
        inotifyFd_ = ::inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
        if (inotifyFd_ < 0) {
            LOG_WARN("vendor-map: inotify_init1 failed, falling back to mtime");
            mode_ = WatchMode::MTime;
            return;
        }

        fs::path dir = activePath_.parent_path();
        if (dir.empty()) dir = ".";
        inotifyWd_ = ::inotify_add_watch(inotifyFd_, dir.string().c_str(),
                                         IN_CLOSE_WRITE | IN_MOVED_TO | IN_ATTRIB |
                                         IN_DELETE | IN_MOVED_FROM | IN_CREATE);
        if (inotifyWd_ < 0) {
            LOG_WARN("vendor-map: inotify_add_watch failed, falling back to mtime");
            ::close(inotifyFd_);
            inotifyFd_ = -1;
            mode_ = WatchMode::MTime;
            return;
        }
        inotifyArmed_ = true;
        LOG_INFO("vendor-map: inotify armed on %s", dir.string().c_str());
    }
#else
    (void)inotifyFd_; (void)inotifyWd_; (void)inotifyArmed_;
    LOG_INFO("vendor-map: inotify unavailable; using mtime");
    mode_ = WatchMode::MTime;
#endif
}

bool VendorMapping::inotifyDrainEventsUnlocked() {
#ifdef __linux__
    if (inotifyFd_ < 0 || inotifyWd_ < 0 || !inotifyArmed_) return false;

    char buf[4096] __attribute__((aligned(__alignof__(struct inotify_event))));
    bool changed = false;

    for (;;) {
        ssize_t len = ::read(inotifyFd_, buf, sizeof(buf));
        if (len < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) break;
            LOG_DEBUG("vendor-map: inotify read error: %d", errno);
            break;
        }
        if (len == 0) break;

        for (char* ptr = buf; ptr < buf + len; ) {
            auto* ev = reinterpret_cast<struct inotify_event*>(ptr);
            ptr += sizeof(struct inotify_event) + ev->len;

            if (ev->len > 0) {
                if (activePath_.filename() == ev->name) changed = true;
            } else {
                changed = true; // conservative
            }
        }
    }
    return changed;
#else
    return false;
#endif
}

void VendorMapping::shutdownInotifyUnlocked() {
#ifdef __linux__
    if (inotifyWd_ >= 0 && inotifyFd_ >= 0) {
        (void)::inotify_rm_watch(inotifyFd_, inotifyWd_);
    }
    if (inotifyFd_ >= 0) ::close(inotifyFd_);
    inotifyFd_ = -1;
    inotifyWd_ = -1;
    inotifyArmed_ = false;
#endif
}

} // namespace lfc
