/*
 * Linux Fan Control — Vendor/Chip/PCI mapping helpers
 * (c) 2025 LinuxFanControl contributors
 */

#include "include/VendorMapping.hpp"
#include "include/Log.hpp"
#include "include/Utils.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;
using nlohmann::json;

namespace lfc {

/* ============================== small utils ============================== */

static inline std::string to_lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return (char)std::tolower(c); });
    return s;
}
static inline std::string trim(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && std::isspace((unsigned char)s[a])) ++a;
    while (b > a && std::isspace((unsigned char)s[b-1])) --b;
    return s.substr(a, b - a);
}
static inline bool hex4(const std::string& s, uint16_t& out) {
    if (s.size() != 4) return false;
    unsigned v = 0;
    if (std::sscanf(s.c_str(), "%x", &v) != 1) return false;
    out = (uint16_t)v;
    return true;
}
static inline long long mtime_seconds(const std::string& path) {
    std::error_code ec;
    auto ft = fs::last_write_time(path, ec);
    if (ec) return 0;
    auto s = std::chrono::time_point_cast<std::chrono::seconds>(ft).time_since_epoch().count();
    return (long long)s;
}

std::string VendorMapping::joinCsv(const std::vector<std::string>& v, const char* sep) {
    std::ostringstream oss;
    for (size_t i = 0; i < v.size(); ++i) { if (i) oss << sep; oss << v[i]; }
    return oss.str();
}
bool VendorMapping::containsI(std::string_view hay, std::string_view needle) {
    auto H = to_lower(std::string(hay));
    auto N = to_lower(std::string(needle));
    return H.find(N) != std::string::npos;
}

/* ============================== JSON loading ============================= */

bool VendorMapping::loadFromPath(const std::string& path) const {
    std::error_code ec;
    if (path.empty() || !fs::exists(path, ec) || ec) {
        return false;
    }

    json j;
    try {
        j = util::read_json_file(path);
    } catch (...) {
        LOG_WARN("vendorMap: failed to parse JSON: %s", path.c_str());
        return false;
    }

    Data newData;

    try {
        if (j.contains("chipVendor") && j["chipVendor"].is_object()) {
            for (auto& [k, v] : j["chipVendor"].items()) {
                if (v.is_string()) newData.chipVendor[to_lower(k)] = v.get<std::string>();
            }
        }
        if (j.contains("chipAliases") && j["chipAliases"].is_object()) {
            for (auto& [k, v] : j["chipAliases"].items()) {
                std::vector<std::string> al;
                if (v.is_array()) {
                    for (auto& a : v) if (a.is_string()) al.push_back(to_lower(a.get<std::string>()));
                } else if (v.is_string()) {
                    std::istringstream iss(v.get<std::string>());
                    std::string tok;
                    while (std::getline(iss, tok, ',')) {
                        tok = trim(tok);
                        if (!tok.empty()) al.push_back(to_lower(tok));
                    }
                }
                newData.chipAliases[to_lower(k)] = std::move(al);
            }
        }
        if (j.contains("chips") && j["chips"].is_object()) {
            for (auto& [chip, obj] : j["chips"].items()) {
                const std::string key = to_lower(chip);
                if (!obj.is_object()) continue;
                if (obj.contains("vendor") && obj["vendor"].is_string()) {
                    newData.chipVendor[key] = obj["vendor"].get<std::string>();
                }
                if (obj.contains("aliases")) {
                    std::vector<std::string> al;
                    const auto& a = obj["aliases"];
                    if (a.is_array()) {
                        for (auto& e : a) if (e.is_string()) al.push_back(to_lower(e.get<std::string>()));
                    } else if (a.is_string()) {
                        std::istringstream iss(a.get<std::string>());
                        std::string tok;
                        while (std::getline(iss, tok, ',')) {
                            tok = trim(tok);
                            if (!tok.empty()) al.push_back(to_lower(tok));
                        }
                    }
                    newData.chipAliases[key] = std::move(al);
                }
            }
        }

        // ---- Optional PCI maps -------------------------------------------
        if (j.contains("pciVendors") && j["pciVendors"].is_object()) {
            for (auto& [k, v] : j["pciVendors"].items()) {
                if (!v.is_string()) continue;
                uint16_t sv = 0;
                if (hex4(to_lower(k), sv)) newData.pciVendorPretty[sv] = v.get<std::string>();
            }
        }
        if (j.contains("pciSubsystems") && j["pciSubsystems"].is_object()) {
            for (auto& [k, v] : j["pciSubsystems"].items()) {
                if (!v.is_string()) continue;
                uint16_t sv = 0;
                if (hex4(to_lower(k), sv)) {
                    if (!newData.pciVendorPretty.count(sv)) {
                        newData.pciVendorPretty[sv] = v.get<std::string>();
                    }
                }
            }
        }
        if (j.contains("pciVendorAliases") && j["pciVendorAliases"].is_object()) {
            for (auto& [from, to] : j["pciVendorAliases"].items()) {
                if (to.is_string()) newData.pciVendorAliases[trim(from)] = to.get<std::string>();
            }
        }
        if (j.contains("pciSubsystemOverrides") && j["pciSubsystemOverrides"].is_object()) {
            for (auto& [k, v] : j["pciSubsystemOverrides"].items()) {
                if (!v.is_string()) continue;
                auto pos = k.find(':');
                if (pos == std::string::npos) continue;
                uint16_t sv = 0, sd = 0;
                if (!hex4(to_lower(k.substr(0, pos)), sv)) continue;
                if (!hex4(to_lower(k.substr(pos + 1)), sd)) continue;
                const uint32_t key = (uint32_t(sv) << 16) | sd;
                newData.pciSubsystemOverrides[key] = v.get<std::string>();
            }
        }
    } catch (...) {
        // tolerate partial loads
    }

    // Swap into live state under lock (no I/O while locked).
    {
        std::lock_guard<std::mutex> lk(mtx_);
        data_ = std::move(newData);
        loadedPath_ = path;
        lastSeenMtime_ = mtime_seconds(path);
    }
    return true;
}

/* ============================== file watching ============================= */

void VendorMapping::ensureLoadedLocked() const {
    // mtx_ must already be held by caller
    if (defaultPath_.empty()) {
        defaultPath_ = util::expandUserPath("~/.config/LinuxFanControl/vendorMapping.json");
    }
    const std::string candidate = !overridePath_.empty() ? overridePath_ : defaultPath_;
    if (!loadedPath_.empty() && loadedPath_ == candidate) {
        return;
    }
    // Release lock while doing I/O
    mtx_.unlock();
    const bool ok = loadFromPath(candidate);
    mtx_.lock();

    if (!ok) {
        loadedPath_ = candidate;
        lastSeenMtime_ = mtime_seconds(candidate);
    }
}

void VendorMapping::pollReloadIfNeededLocked() const {
    // mtx_ must already be held by caller
    if (watchMode_ != WatchMode::MTime) return;
    const auto now = std::chrono::steady_clock::now();
    if (lastPoll_.time_since_epoch().count() != 0) {
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastPoll_);
        if (elapsed.count() < std::max(0, watchThrottleMs_)) return;
    }
    lastPoll_ = now;

    if (loadedPath_.empty()) return;
    const std::string path = loadedPath_; // copy while locked
    const long long prev = lastSeenMtime_;

    // Release lock for stat + potential load
    mtx_.unlock();
    const long long mt = mtime_seconds(path);
    const bool changed = (mt > 0 && mt != prev);
    if (changed) (void)loadFromPath(path);
    mtx_.lock();

    if (changed) {
        LOG_DEBUG("vendorMap: reloaded: %s", path.c_str());
    }
}

/* ============================== singleton/ctor ============================ */

std::once_flag VendorMapping::s_pciOnce_;
std::mutex     VendorMapping::s_pciMx_;
bool           VendorMapping::s_pciLoaded_ = false;
std::unordered_map<uint16_t, std::string> VendorMapping::s_pciVendorNames_;
std::unordered_map<uint32_t, std::string> VendorMapping::s_pciSubsysNames_;

VendorMapping& VendorMapping::instance() {
    static VendorMapping s;
    return s;
}
VendorMapping::VendorMapping() {
    defaultPath_ = util::expandUserPath("~/.config/LinuxFanControl/vendorMapping.json");
    loadedPath_.clear();
    lastSeenMtime_ = 0;
    lastPoll_ = {};
}

/* ============================== config ==================================== */

void VendorMapping::setOverridePath(const std::string& path) {
    std::lock_guard<std::mutex> lk(mtx_);
    overridePath_ = util::expandUserPath(path);
    loadedPath_.clear(); // force reload on next query
}
void VendorMapping::setWatchMode(WatchMode mode, int throttleMs) {
    std::lock_guard<std::mutex> lk(mtx_);
    watchMode_ = mode;
    watchThrottleMs_ = std::max(0, throttleMs);
}

/* ============================== public lookups ============================ */

std::string VendorMapping::vendorForChipName(const std::string& chip) {
    std::lock_guard<std::mutex> lk(mtx_);
    ensureLoadedLocked();
    pollReloadIfNeededLocked();

    if (chip.empty()) return {};
    const std::string key = to_lower(chip);

    if (auto it = data_.chipVendor.find(key); it != data_.chipVendor.end()) {
        return it->second;
    }

    for (const auto& kv : data_.chipAliases) {
        const auto& canonical = kv.first;
        const auto& aliases   = kv.second;
        if (std::find(aliases.begin(), aliases.end(), key) != aliases.end()) {
            if (auto it = data_.chipVendor.find(canonical); it != data_.chipVendor.end())
                return it->second;
            return canonical;
        }
    }

    if (containsI(key, "amdgpu"))  return "AMD GPU";
    if (containsI(key, "nvidia"))  return "NVIDIA GPU";
    if (containsI(key, "i915") || containsI(key, "intel") || containsI(key, "xe")) return "Intel GPU";
    if (containsI(key, "nvme"))    return "NVMe Drive";
    if (containsI(key, "k10temp")) return "AMD CPU (Zen/K10)";

    return chip;
}

std::vector<std::string> VendorMapping::chipAliasesFor(const std::string& chip) {
    std::lock_guard<std::mutex> lk(mtx_);
    ensureLoadedLocked();
    pollReloadIfNeededLocked();

    std::vector<std::string> out;
    if (chip.empty()) return out;

    const std::string key = to_lower(chip);

    if (auto it = data_.chipAliases.find(key); it != data_.chipAliases.end()) {
        out = it->second;
        out.insert(out.begin(), key);
        return out;
    }

    for (const auto& kv : data_.chipAliases) {
        const auto& canonical = kv.first;
        const auto& aliases   = kv.second;
        if (std::find(aliases.begin(), aliases.end(), key) != aliases.end()) {
            out = aliases;
            out.insert(out.begin(), canonical);
            return out;
        }
    }

    out.push_back(chip);
    return out;
}

std::string VendorMapping::aliasesJoinForLog(const std::string& chip) const {
    auto a = const_cast<VendorMapping*>(this)->chipAliasesFor(chip);
    return joinCsv(a, ",");
}

/* ============================== GPU helpers =============================== */

std::string VendorMapping::gpuCanonicalVendor(std::string_view s) const {
    if (containsI(s, "nvidia") || containsI(s, "nvml")) return "NVIDIA";
    if (containsI(s, "intel")  || containsI(s, "igcl") || containsI(s, "level zero")) return "Intel";
    if (containsI(s, "amd")    || containsI(s, "amdsmi") || containsI(s, "radeon"))   return "AMD";
    return "Unknown";
}

std::pair<std::string, std::string>
VendorMapping::gpuVendorAndKindFromIdentifier(const std::string& identifier) const {
    const std::string ven  = gpuCanonicalVendor(identifier);
    std::string kind = "Unknown";
    const auto lid = to_lower(identifier);
    if (lid.find("hotspot") != std::string::npos || lid.find("junction") != std::string::npos) kind = "Hotspot";
    else if (lid.find("mem") != std::string::npos || lid.find("vram") != std::string::npos)   kind = "Memory";
    else if (lid.find("edge") != std::string::npos || lid.find("/temp/gpu") != std::string::npos || lid.find("gpu") != std::string::npos) kind = "Edge";
    return { ven, kind };
}

/* ============================== pci.ids loader ============================ */

static inline std::string trimLeftTabs(const std::string& s, size_t& tabsOut) {
    size_t tabs = 0;
    while (tabs < s.size() && s[tabs] == '\t') ++tabs;
    tabsOut = tabs;
    return s.substr(tabs);
}

void VendorMapping::ensurePciDbLoaded() {
    std::call_once(s_pciOnce_, []{
        std::lock_guard<std::mutex> lk(s_pciMx_);
        const char* candidates[] = {
            "/usr/share/hwdata/pci.ids",
            "/usr/share/misc/pci.ids",
            "/usr/share/libpci/pci.ids",
            nullptr
        };
        std::ifstream f;
        for (const char** p = candidates; *p; ++p) {
            f.open(*p);
            if (f.is_open()) break;
        }
        if (!f.is_open()) {
            s_pciLoaded_ = true;
            return;
        }

        std::string line;
        uint16_t curVendor = 0;

        while (std::getline(f, line)) {
            if (line.empty() || line[0] == '#') continue;
            size_t tabs = 0;
            std::string rest = trim(trimLeftTabs(line, tabs));

            if (tabs == 0) {
                std::istringstream iss(rest);
                std::string vhex, name;
                iss >> vhex;
                std::getline(iss, name);
                uint16_t v;
                if (hex4(to_lower(vhex), v)) {
                    curVendor = v;
                    s_pciVendorNames_[v] = trim(name);
                }
            } else if (tabs == 2) {
                std::istringstream iss(rest);
                std::string svHex, sdHex;
                iss >> svHex >> sdHex;
                uint16_t sv = 0, sd = 0;
                if (hex4(to_lower(svHex), sv) && hex4(to_lower(sdHex), sd)) {
                    std::string subsysName;
                    std::getline(iss, subsysName);
                    const uint32_t key = (uint32_t(sv) << 16) | sd;
                    s_pciSubsysNames_[key] = trim(subsysName);
                }
            }
        }

        s_pciLoaded_ = true;
    });
}

std::string VendorMapping::pciIdsVendorName(uint16_t sv) {
    ensurePciDbLoaded();
    std::lock_guard<std::mutex> lk(s_pciMx_);
    auto it = s_pciVendorNames_.find(sv);
    if (it != s_pciVendorNames_.end()) return it->second;
    char buf[16]; std::snprintf(buf, sizeof(buf), "sv%04x", sv);
    return std::string(buf);
}
std::string VendorMapping::pciIdsSubsystemName(uint16_t sv, uint16_t sd) {
    ensurePciDbLoaded();
    std::lock_guard<std::mutex> lk(s_pciMx_);
    const uint32_t key = (uint32_t(sv) << 16) | sd;
    auto it = s_pciSubsysNames_.find(key);
    if (it != s_pciSubsysNames_.end()) return it->second;
    return {};
}

/* ============================== PCI helpers =============================== */

std::string VendorMapping::pciVendorName(uint16_t subsystemVendorId) const {
    // Avoid lock inversion: fetch pci.ids name first (locks s_pciMx_), then apply JSON/aliases under mtx_.
    const std::string idsName = pciIdsVendorName(subsystemVendorId);

    std::lock_guard<std::mutex> lk(mtx_);
    ensureLoadedLocked();
    pollReloadIfNeededLocked();

    // JSON pretty override?
    if (auto it = data_.pciVendorPretty.find(subsystemVendorId); it != data_.pciVendorPretty.end()) {
        const std::string raw = it->second;
        if (auto al = data_.pciVendorAliases.find(raw); al != data_.pciVendorAliases.end()) return al->second;
        return raw;
    }

    // Apply alias on pci.ids name (if configured)
    if (auto al = data_.pciVendorAliases.find(idsName); al != data_.pciVendorAliases.end()) return al->second;
    return idsName;
}

std::string VendorMapping::boardForSubsystem(uint16_t subsystemVendorId,
                                             uint16_t subsystemDeviceId) const {
    // Avoid lock inversion: read pci.ids subsystem name first, then consult JSON overrides.
    const std::string idsName = pciIdsSubsystemName(subsystemVendorId, subsystemDeviceId);

    std::lock_guard<std::mutex> lk(mtx_);
    ensureLoadedLocked();
    pollReloadIfNeededLocked();

    const uint32_t key = (uint32_t(subsystemVendorId) << 16) | subsystemDeviceId;

    if (auto it = data_.pciSubsystemOverrides.find(key); it != data_.pciSubsystemOverrides.end()) {
        return it->second;
    }
    return idsName;
}

} // namespace lfc
