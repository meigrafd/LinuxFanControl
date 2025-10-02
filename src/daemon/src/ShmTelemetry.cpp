/*
 * Linux Fan Control — SHM Telemetry Publisher (implementation)
 * - Builds a compact JSON snapshot of hwmon + GPU + profile meta
 * - Publishes to POSIX shared memory (shm) with /dev/shm fallback file
 * - Lightweight write guard: skip writes if payload unchanged
 * (c) 2025 LinuxFanControl contributors
 */

#include "include/ShmTelemetry.hpp"
#include "include/Hwmon.hpp"
#include "include/GpuMonitor.hpp"
#include "include/Profile.hpp"
#include "include/Version.hpp"
#include "include/Log.hpp"

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <chrono>
#include <cmath>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include <memory>
#include <algorithm>
#include <functional>

#include <nlohmann/json.hpp>

namespace lfc {

using nlohmann::json;

// ============================================================================
// Small helpers
// ============================================================================

static inline std::string base_name(const std::string& p) {
    auto pos = p.find_last_of("/\\");
    return (pos == std::string::npos) ? p : p.substr(pos + 1);
}

static inline bool starts_with(const std::string& s, const char* prefix) {
    const size_t n = std::strlen(prefix);
    return s.size() >= n && std::memcmp(s.data(), prefix, n) == 0;
}

static inline long long now_unix_ms() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

// Optional→JSON setter (nur wenn vorhanden)
template <typename T>
static inline void jset(json& j, std::string key, const std::optional<T>& v) {
    if (v) j[std::move(key)] = *v;
}

// ============================================================================
// JSON builders (nur Felder, die es laut Headers wirklich gibt)
// ============================================================================

static json jChip(const HwmonChip& c) {
    json j;
    j["path"]   = c.hwmonPath;  // z.B. /sys/class/hwmon/hwmon4
    if (!c.name.empty())   j["name"]   = c.name;
    if (!c.vendor.empty()) j["vendor"] = c.vendor;
    return j;
}

static json jHwmonTemp(const HwmonTemp& t) {
    json j;
    j["chipPath"]   = t.chipPath;
    j["inputPath"]  = t.path_input;
    if (!t.label.empty()) j["label"] = t.label;

    if (auto v = Hwmon::readTempC(t)) j["valueC"] = *v;
    return j;
}

static json jHwmonFan(const HwmonFan& f) {
    json j;
    j["chipPath"]   = f.chipPath;
    j["inputPath"]  = f.path_input;
    if (!f.label.empty()) j["label"] = f.label;

    if (auto rpm = Hwmon::readRpm(f)) j["rpm"] = *rpm;
    return j;
}

static inline int parse_index_after_prefix(const std::string& base, const char* prefix) {
    if (base.rfind(prefix, 0) != 0) return -1;
    size_t i = std::strlen(prefix);
    int idx = 0; bool any=false;
    while (i < base.size() && std::isdigit((unsigned char)base[i])) { any=true; idx = idx*10 + (base[i]-'0'); ++i; }
    return any ? idx : -1;
}

static std::optional<int> rpmForPwm(const HwmonPwm& p, const std::vector<HwmonFan>& fans) {
    const std::string pwmBase = base_name(p.path_pwm); // "pwmN"
    const int idx = parse_index_after_prefix(pwmBase, "pwm");
    if (idx <= 0) return std::nullopt;

    for (const auto& f : fans) {
        if (f.chipPath != p.chipPath) continue;
        const std::string fanBase = base_name(f.path_input); // "fanN_input"
        const int findex = parse_index_after_prefix(fanBase, "fan");
        if (findex == idx) {
            if (auto rpm = Hwmon::readRpm(f)) return rpm;
            break;
        }
    }
    return std::nullopt;
}

static json jHwmonPwm(const HwmonPwm& p, const std::vector<HwmonFan>& fans) {
    json j;
    j["chipPath"]  = p.chipPath;
    j["pwmPath"]   = p.path_pwm;
    if (!p.path_enable.empty()) j["enablePath"] = p.path_enable;
    j["pwmMax"]    = p.pwm_max;

    if (auto en  = Hwmon::readEnable(p)) j["enable"] = *en;
    if (auto raw = Hwmon::readRaw(p)) {
        j["raw"] = *raw;
        const int vmax = std::max(1, p.pwm_max);
        j["percent"] = (int)std::lround(100.0 * (double)*raw / (double)vmax);
    }
    if (auto rpm = rpmForPwm(p, fans)) j["fanRpm"] = *rpm;

    return j;
}

static json jGpu(const GpuSample& g) {
    json j;
    if (!g.vendor.empty())    j["vendor"] = g.vendor;
    j["index"] = g.index;
    if (!g.name.empty())      j["name"]   = g.name;
    if (!g.pciBusId.empty())  j["pci"]    = g.pciBusId;
    if (!g.drmCard.empty())   j["drm"]    = g.drmCard;
    if (!g.hwmonPath.empty()) j["hwmon"]  = g.hwmonPath;

    j["hasFanTach"] = g.hasFanTach;
    j["hasFanPwm"]  = g.hasFanPwm;

    jset(j, "fanRpm",       g.fanRpm);
    jset(j, "tempEdgeC",    g.tempEdgeC);
    jset(j, "tempHotspotC", g.tempHotspotC);
    jset(j, "tempMemoryC",  g.tempMemoryC);
    return j;
}

static json jProfileSummary(const Profile& p) {
    json j;
    if (!p.name.empty()) j["name"] = p.name;

    // Controls (PWM-Zuordnungen)
    {
        json arr = json::array();
        for (const auto& c : p.controls) {
            json cj;
            if (!c.name.empty())     cj["name"]     = c.name;
            if (!c.pwmPath.empty())  cj["pwmPath"]  = c.pwmPath;
            if (!c.curveRef.empty()) cj["curveRef"] = c.curveRef;
            if (!c.nickName.empty()) cj["nick"]     = c.nickName;
            arr.push_back(std::move(cj));
        }
        j["controls"]     = std::move(arr);
        j["controlCount"] = p.controls.size();
    }

    // FanCurves (nur Meta, Felder laut FanCurveMeta)
    {
        json arr = json::array();
        for (const auto& fc : p.fanCurves) {
            json cj;
            if (!fc.name.empty()) cj["name"] = fc.name;
            if (!fc.type.empty()) cj["type"] = fc.type;
            if (!fc.tempSensors.empty()) cj["tempSensors"] = fc.tempSensors;
            if (!fc.points.empty()) cj["pointsCount"] = fc.points.size();
            // optional: trigger-Infos, falls gesetzt
            if (fc.onC != 0.0 || fc.offC != 0.0) {
                cj["trigger"] = json{{"onC", fc.onC}, {"offC", fc.offC}};
            }
            arr.push_back(std::move(cj));
        }
        j["fanCurves"]   = std::move(arr);
        j["curveCount"]  = p.fanCurves.size();
    }

    // Profile-seitig bekannte Hwmon-Geräte
    if (!p.hwmons.empty()) {
        json arr = json::array();
        for (const auto& h : p.hwmons) {
            json hj;
            if (!h.hwmonPath.empty()) hj["hwmonPath"] = h.hwmonPath;
            if (!h.name.empty())      hj["name"]      = h.name;
            if (!h.vendor.empty())    hj["vendor"]    = h.vendor;
            if (!h.pwms.empty())      hj["pwmCount"]  = h.pwms.size();
            arr.push_back(std::move(hj));
        }
        j["hwmons"] = std::move(arr);
    }

    return j;
}

// ============================================================================
// Impl — vollständig vor den Methoden (vermeidet incomplete-type Fehler)
// ============================================================================

struct ShmTelemetryImpl {
    std::string shmName;      // z.B. "/lfc.telemetry"
    std::string fallbackPath; // z.B. "/dev/shm/lfc.telemetry"

    // Lightweight write guard (Signatur basierend auf Payload ohne timestamp)
    std::string lastSig;
    size_t      lastSize{0};
    size_t      lastHash{0};

    static std::pair<std::string,std::string> normalize(const std::string& shmNameOrPath,
                                                        const std::string& explicitFallback = {}) {
        if (!explicitFallback.empty()) {
            std::string nm = shmNameOrPath;
            if (!nm.empty() && nm[0] != '/') nm.insert(nm.begin(), '/');
            std::string fb = explicitFallback;
            if (fb.empty()) fb = "/dev/shm/" + base_name(nm.substr(1));
            return {nm, fb};
        }

        std::string nm;
        std::string fb;

        if (shmNameOrPath.empty()) {
            nm = "/lfc.telemetry";
            fb = "/dev/shm/lfc.telemetry";
            return {nm, fb};
        }

        if (shmNameOrPath[0] == '/') {
            if (starts_with(shmNameOrPath, "/dev/shm/")) {
                fb = shmNameOrPath;
                nm = "/" + base_name(fb);         // SHM-Name aus Basename
            } else {
                nm = shmNameOrPath;               // treat as shm name
                fb = "/dev/shm/" + base_name(nm.substr(1));
            }
        } else {
            nm = "/" + shmNameOrPath;             // shm name
            fb = "/dev/shm/" + shmNameOrPath;
        }
        return {nm, fb};
    }

    bool writePayload(const std::string& payload, json* details) {
        const size_t size = payload.size();

        // 1) POSIX SHM
        int fd = shm_open(shmName.c_str(), O_CREAT | O_RDWR, 0660);
        if (fd >= 0) {
            bool ok = true;
            if (ftruncate(fd, static_cast<off_t>(size)) != 0) {
                ok = false;
                if (details) (*details)["warn"] = std::string("ftruncate errno=") + std::to_string(errno);
            } else {
                ssize_t wrote = 0;
                const char* p = payload.data();
                ssize_t n = (ssize_t)size;
                while (n > 0) {
                    ssize_t w = ::write(fd, p, n);
                    if (w < 0) {
                        if (errno == EINTR) continue;
                        ok = false;
                        if (details) (*details)["warn"] = std::string("write errno=") + std::to_string(errno);
                        break;
                    }
                    wrote += w; p += w; n -= w;
                }
                (void)wrote;
            }
            ::close(fd);
            if (ok) return true;
        } else {
            if (details) (*details)["warn"] = std::string("shm_open errno=") + std::to_string(errno);
        }

        // 2) Fallback: /dev/shm Datei (atomar via tmp + rename)
        const std::string tmp = fallbackPath + ".tmp";
        {
            std::ofstream ofs(tmp, std::ios::binary | std::ios::trunc);
            if (!ofs) { if (details) (*details)["error"] = "fallback open failed"; return false; }
            ofs.write(payload.data(), (std::streamsize)payload.size());
            if (!ofs) { if (details) (*details)["error"] = "fallback write failed"; return false; }
        }
        if (::rename(tmp.c_str(), fallbackPath.c_str()) != 0) {
            if (details) (*details)["error"] = std::string("rename errno=") + std::to_string(errno);
            return false;
        }
        return true;
    }

    bool shouldWrite(const std::string& payload, json* details) {
        const size_t sz = payload.size();
        const size_t h  = std::hash<std::string_view>{}(std::string_view(payload));
        if (sz == lastSize && h == lastHash && payload == lastSig) {
            if (details) (*details)["skipped"] = "unchanged";
            return false;
        }
        return true;
    }

    void remember(const std::string& payload) {
        lastSig  = payload;
        lastSize = payload.size();
        lastHash = std::hash<std::string_view>{}(std::string_view(lastSig));
    }
};

// ============================================================================
// ShmTelemetry — public API
// ============================================================================

ShmTelemetry::ShmTelemetry(const std::string& shmNameOrPath) {
    auto [nm, fb] = ShmTelemetryImpl::normalize(shmNameOrPath);
    impl_ = std::make_unique<ShmTelemetryImpl>();
    impl_->shmName      = std::move(nm);
    impl_->fallbackPath = std::move(fb);
}

ShmTelemetry::ShmTelemetry(const std::string& shmName, const std::string& fallbackPath) {
    auto [nm, fb] = ShmTelemetryImpl::normalize(shmName, fallbackPath);
    impl_ = std::make_unique<ShmTelemetryImpl>();
    impl_->shmName      = std::move(nm);
    impl_->fallbackPath = std::move(fb);
}

//ShmTelemetry::~ShmTelemetry() = default;
ShmTelemetry::~ShmTelemetry() {
    if (!impl_) return;
    json j;
    j["version"]       = LFCD_VERSION;
    j["timestampMs"]   = now_unix_ms();
    j["engineEnabled"] = false;
    // write directly (bypass guard)
    (void)impl_->writePayload(j.dump(), nullptr);
}

// ---- Snapshot Publisher (nur hwmon) ----------------------------------------

bool ShmTelemetry::publishSnapshot(const HwmonInventory& inv, json* detailsOut) {
    static const std::vector<GpuSample> emptyGpu;
    static const Profile emptyProfile;
    return publishSnapshot(inv, emptyGpu, emptyProfile, /*engineEnabled*/false, detailsOut);
}

// ---- Snapshot Publisher (hwmon + gpu + profile + engineEnabled) ------------

static json build_top(const HwmonInventory& inv,
                      const std::vector<GpuSample>& gpus,
                      const Profile& profile,
                      bool engineEnabled)
{
    json j;
    j["version"]      = LFCD_VERSION;
    j["timestampMs"]  = now_unix_ms();
    j["engineEnabled"] = engineEnabled;

    // hwmon chips
    {
        json arr = json::array();
        for (const auto& c : inv.chips) arr.push_back(jChip(c));
        j["chips"] = std::move(arr);
    }
    // temps
    {
        json arr = json::array();
        for (const auto& t : inv.temps) arr.push_back(jHwmonTemp(t));
        j["temps"] = std::move(arr);
    }
    // fans
    {
        json arr = json::array();
        for (const auto& f : inv.fans) arr.push_back(jHwmonFan(f));
        j["fans"] = std::move(arr);
    }
    // pwms
    {
        json arr = json::array();
        for (const auto& p : inv.pwms) arr.push_back(jHwmonPwm(p, inv.fans));
        j["pwms"] = std::move(arr);
    }
    // gpus
    {
        json arr = json::array();
        for (const auto& g : gpus) arr.push_back(jGpu(g));
        j["gpus"] = std::move(arr);
    }
    // profile summary (kompakt – Details via RPC)
    j["profile"] = jProfileSummary(profile);

    return j;
}

bool ShmTelemetry::publishSnapshot(const HwmonInventory& inv,
                                   const std::vector<GpuSample>& gpus,
                                   const Profile& profile,
                                   bool engineEnabled,
                                   json* detailsOut)
{
    json details;
    // 1) JSON bauen
    json j = build_top(inv, gpus, profile, engineEnabled);

    // 2) Signatur ohne timestamp (Lightweight-Guard)
    json sig = j;
    sig.erase("timestampMs");
    const std::string sigStr = sig.dump();

    if (!impl_->shouldWrite(sigStr, &details)) {
        if (detailsOut) *detailsOut = std::move(details);
        return true;
    }

    // 3) Finale Nutzlast
    const std::string payload = j.dump(); // kompakt

    const bool ok = impl_->writePayload(payload, &details);
    if (ok) impl_->remember(sigStr);

    if (detailsOut) *detailsOut = std::move(details);
    return ok;
}

// ============================================================================
// Statische Build-JSON-API (wie in Header deklariert)
// ============================================================================

json ShmTelemetry::buildJson(const HwmonInventory& inv,
                             const std::vector<GpuSample>& gpus,
                             const Profile& profile,
                             bool engineEnabled)
{
    return build_top(inv, gpus, profile, engineEnabled);
}

} // namespace lfc
