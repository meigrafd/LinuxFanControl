/*
 * Linux Fan Control — Telemetry (implementation)
 * Publishes a compact JSON snapshot of engine/gpu/hwmon state to shared memory.
 */

#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <system_error>
#include <vector>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <nlohmann/json.hpp>

#include "include/Version.hpp"
#include "include/Log.hpp"
#include "include/Utils.hpp"
#include "include/Hwmon.hpp"
#include "include/GpuMonitor.hpp"
#include "include/Profile.hpp"
#include "include/ShmTelemetry.hpp"

namespace lfc {

using nlohmann::json;
namespace fs = std::filesystem;

/* =============================== helpers ================================== */

static std::string readSmallTextFile(const std::string& path) {
    std::ifstream f(path);
    if (!f) return {};
    std::string s;
    std::getline(f, s);
    while (!s.empty() && (s.back() == '\r' || s.back() == '\n' || s.back() == ' ' || s.back() == '\t'))
        s.pop_back();
    return s;
}

static bool fileExists(const std::string& path) {
    std::error_code ec;
    return fs::exists(path, ec);
}

template <class T>
static inline void jset(json& j, const char* key, const std::optional<T>& v) {
    if (v.has_value()) j[key] = *v;
}

// Extract N from basename matching prefix+N (e.g. "pwm7", "fan3_input")
static std::optional<int> extractIndexFromNode(const std::string& nodePath, const char* prefix) {
    auto pos = nodePath.find_last_of('/');
    if (pos == std::string::npos) return std::nullopt;
    const char* base = nodePath.c_str() + pos + 1;
    const size_t preLen = std::strlen(prefix);
    if (std::strncmp(base, prefix, preLen) != 0) return std::nullopt;

    int idx = 0;
    const char* p = base + preLen;
    if (*p < '0' || *p > '9') return std::nullopt;
    while (*p >= '0' && *p <= '9') {
        idx = (idx * 10) + (*p - '0');
        ++p;
    }
    return idx > 0 ? std::optional<int>(idx) : std::nullopt;
}

static std::string derivePwmLabel(const std::string& pwmPath) {
    auto idxOpt = extractIndexFromNode(pwmPath, "pwm");
    if (!idxOpt) return {};
    int idx = *idxOpt;
    const auto dir = fs::path(pwmPath).parent_path().string();

    const std::string pwmLabel = dir + "/pwm" + std::to_string(idx) + "_label";
    if (fileExists(pwmLabel)) {
        auto s = readSmallTextFile(pwmLabel);
        if (!s.empty()) return s;
    }
    const std::string fanLabel = dir + "/fan" + std::to_string(idx) + "_label";
    if (fileExists(fanLabel)) {
        auto s = readSmallTextFile(fanLabel);
        if (!s.empty()) return s;
    }
    return {};
}

static std::string deriveFanLabel(const std::string& fanInputPath) {
    auto idxOpt = extractIndexFromNode(fanInputPath, "fan");
    if (!idxOpt) return {};
    int idx = *idxOpt;
    const auto dir = fs::path(fanInputPath).parent_path().string();

    const std::string fanLabel = dir + "/fan" + std::to_string(idx) + "_label";
    if (fileExists(fanLabel)) {
        auto s = readSmallTextFile(fanLabel);
        if (!s.empty()) return s;
    }
    return {};
}

/* ============================== json builders ============================== */

static json jChip(const HwmonChip& c) {
    return json{
        {"name",      c.name},
        {"vendor",    c.vendor},
        {"hwmonPath", c.hwmonPath}
    };
}

static json jHwmonTemp(const HwmonTemp& t) {
    return json{
        {"path",     t.path_input},
        {"label",    t.label},
        {"chipPath", t.chipPath}
    };
}

static json jHwmonFan(const HwmonFan& f) {
    std::string label = f.label;
    if (label.empty()) {
        auto d = deriveFanLabel(f.path_input);
        if (!d.empty()) label = std::move(d);
    }
    return json{
        {"path",     f.path_input},
        {"label",    label},
        {"chipPath", f.chipPath}
    };
}

static json jHwmonPwm(const HwmonPwm& p) {
    std::string label = p.label;
    if (label.empty()) {
        auto d = derivePwmLabel(p.path_pwm);
        if (!d.empty()) label = std::move(d);
    }
    return json{
        {"path",       p.path_pwm},
        {"pathEnable", p.path_enable},
        {"pwmMax",     p.pwm_max},
        {"label",      label},
        {"chipPath",   p.chipPath}
    };
}

static json jGpu(const GpuSample& g) {
    json j{
        {"vendor",     g.vendor},
        {"index",      g.index},
        {"name",       g.name},
        {"pci",        g.pciBusId},
        {"drm",        g.drmCard},
        {"hwmon",      g.hwmonPath},
        {"hasFanTach", g.hasFanTach ? 1 : 0},
        {"hasFanPwm",  g.hasFanPwm  ? 1 : 0}
    };
    // Optionals
    jset(j, "fanRpm",       g.fanRpm);       // std::optional<int>
    jset(j, "tempEdgeC",    g.tempEdgeC);    // std::optional<double>
    jset(j, "tempHotspotC", g.tempHotspotC);
    jset(j, "tempMemoryC",  g.tempMemoryC);
    return j;
}

/* ============================== top-level json ============================= */

json ShmTelemetry::buildJson(const HwmonInventory& hw,
                             const std::vector<GpuSample>& gpus,
                             const Profile& profile,
                             bool engineEnabled)
{
    json j;
    j["version"] = LFCD_VERSION;
    j["engineEnabled"] = engineEnabled;

    // hwmon
    {
        json jchips = json::array();
        for (const auto& c : hw.chips) jchips.push_back(jChip(c));
        json jtemps = json::array();
        for (const auto& t : hw.temps) jtemps.push_back(jHwmonTemp(t));
        json jfans = json::array();
        for (const auto& f : hw.fans)  jfans.push_back(jHwmonFan(f));
        json jpwms = json::array();
        for (const auto& p : hw.pwms)  jpwms.push_back(jHwmonPwm(p));

        j["hwmon"] = json{
            {"chips", jchips},
            {"temps", jtemps},
            {"fans",  jfans},
            {"pwms",  jpwms}
        };
    }

    // gpus
    {
        json jg = json::array();
        for (const auto& g : gpus) jg.push_back(jGpu(g));
        j["gpus"] = std::move(jg);
    }

    // profile summary (lightweight)
    {
        json jp{
            {"name",         profile.name},
            {"description",  profile.description},
            {"schema",       profile.schema},
            {"curveCount",   static_cast<int>(profile.fanCurves.size())},
            {"controlCount", static_cast<int>(profile.controls.size())}
        };
        j["profile"] = std::move(jp);
    }

    return j;
}

/* =============================== SHM writer ================================ */

static std::string normalizeShmName(const std::string& pathOrName) {
    if (pathOrName.empty()) return "/lfc.telemetry";
    if (pathOrName[0] != '/') return "/" + pathOrName;
    if (pathOrName.rfind("/dev/shm/", 0) == 0) return {};
    return pathOrName;
}

static std::string defaultFallbackForShm(const std::string& shmNameNormalized) {
    std::string name = shmNameNormalized;
    if (!name.empty() && name.front() == '/') name.erase(0,1);
    if (name.empty()) name = "lfc.telemetry";
    return "/dev/shm/" + name;
}

struct ShmTelemetryImpl {
    explicit ShmTelemetryImpl(const std::string& shmNameOrPath) {
        if (!shmNameOrPath.empty() &&
            shmNameOrPath[0] == '/' &&
            shmNameOrPath.rfind("/dev/shm/", 0) != 0 &&
            shmNameOrPath.find('/', 1) == std::string::npos) {
            shmName_ = shmNameOrPath;
            fallbackPath_ = defaultFallbackForShm(shmName_);
        } else if (!shmNameOrPath.empty() && shmNameOrPath[0] != '/') {
            shmName_ = "/" + shmNameOrPath;
            fallbackPath_ = defaultFallbackForShm(shmName_);
        } else {
            fallbackPath_ = shmNameOrPath.empty() ? defaultFallbackForShm("/lfc.telemetry") : shmNameOrPath;
        }
        LOG_INFO("telemetry: shm=%s fallback=%s",
                 shmName_.empty() ? "<disabled>" : shmName_.c_str(),
                 fallbackPath_.empty() ? "<disabled>" : fallbackPath_.c_str());
    }

    ShmTelemetryImpl(const std::string& shmName, const std::string& fallbackPath) {
        shmName_     = normalizeShmName(shmName);
        fallbackPath_ = fallbackPath.empty()
                      ? defaultFallbackForShm(shmName_.empty() ? "/lfc.telemetry" : shmName_)
                      : fallbackPath;
        LOG_INFO("telemetry: shm=%s fallback=%s",
                 shmName_.empty() ? "<disabled>" : shmName_.c_str(),
                 fallbackPath_.empty() ? "<disabled>" : fallbackPath_.c_str());
    }

    bool publishToShm(const std::string& text) {
        if (shmName_.empty()) return false;

        int fd = shm_open(shmName_.c_str(), O_CREAT | O_RDWR, 0660);
        if (fd < 0) {
            LOG_WARN("telemetry: shm_open(%s) failed: %s", shmName_.c_str(), std::strerror(errno));
            return false;
        }

        const size_t size = text.size() + 1;
        if (ftruncate(fd, size) != 0) {
            LOG_WARN("telemetry: ftruncate failed: %s", std::strerror(errno));
            close(fd);
            return false;
        }

        void* addr = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        if (addr == MAP_FAILED) {
            LOG_WARN("telemetry: mmap failed: %s", std::strerror(errno));
            close(fd);
            return false;
        }

        std::memcpy(addr, text.data(), text.size());
        static_cast<char*>(addr)[text.size()] = '\0';

        msync(addr, size, MS_SYNC);
        munmap(addr, size);
        close(fd);
        return true;
    }

    bool publishToFile(const std::string& path, const std::string& text) {
        if (path.empty()) return false;
        std::error_code ec;
        fs::path p(path);
        if (p.has_parent_path()) fs::create_directories(p.parent_path(), ec);
        std::ofstream f(path, std::ios::binary | std::ios::trunc);
        if (!f) {
            LOG_WARN("telemetry: write(%s) failed", path.c_str());
            return false;
        }
        f.write(text.data(), static_cast<std::streamsize>(text.size()));
        f.flush();
        return true;
    }

    std::string shmName_;      // "/lfc.telemetry" or empty if path-only
    std::string fallbackPath_; // "/dev/shm/lfc.telemetry" or explicit path
};

/* =============================== API impl ================================= */

ShmTelemetry::ShmTelemetry(const std::string& shmNameOrPath)
: impl_(new ShmTelemetryImpl(shmNameOrPath)) {}

ShmTelemetry::ShmTelemetry(const std::string& shmName, const std::string& fallbackPath)
: impl_(new ShmTelemetryImpl(shmName, fallbackPath)) {}

ShmTelemetry::~ShmTelemetry() = default;

bool ShmTelemetry::publishSnapshot(const HwmonInventory& inv) {
    return publishSnapshot(inv, static_cast<json*>(nullptr));
}

bool ShmTelemetry::publishSnapshot(const HwmonInventory& inv, json* detailsOut) {
    static const std::vector<GpuSample> kEmptyGpus;
    static const Profile kEmptyProfile{};
    return publishSnapshot(inv, kEmptyGpus, kEmptyProfile, false, detailsOut);
}

bool ShmTelemetry::publishSnapshot(const HwmonInventory& inv,
                                   const std::vector<GpuSample>& gpus,
                                   const Profile& profile,
                                   bool engineEnabled)
{
    return publishSnapshot(inv, gpus, profile, engineEnabled, static_cast<json*>(nullptr));
}

bool ShmTelemetry::publishSnapshot(const HwmonInventory& inv,
                                   const std::vector<GpuSample>& gpus,
                                   const Profile& profile,
                                   bool engineEnabled,
                                   json* detailsOut)
{
    json j = buildJson(inv, gpus, profile, engineEnabled);
    if (detailsOut) *detailsOut = j;

    std::string dump = j.dump();

    bool ok = false;
    if (impl_->publishToShm(dump)) ok = true;
    else LOG_INFO("telemetry: shm publish failed, trying file fallback");

    if (!impl_->fallbackPath_.empty()) {
        if (impl_->publishToFile(impl_->fallbackPath_, dump)) ok = true;
    }
    return ok;
}

} // namespace lfc
