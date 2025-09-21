/*
 * Linux Fan Control — Telemetry (POSIX shared memory)
 * (c) 2025 LinuxFanControl contributors
 */
#include "include/ShmTelemetry.hpp"
#include "include/Utils.hpp"
#include "include/Log.hpp"
#include "include/Version.hpp"

#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

namespace lfc {

// ------------------------------- helpers -------------------------------------

static void ensure_dir_for_file(const std::string& path) {
    std::error_code ec;
    fs::path p(path);
    if (!p.parent_path().empty()) {
        fs::create_directories(p.parent_path(), ec);
    }
}

std::string ShmTelemetry::toShmName(const std::string& pathOrName) {
    // If it contains '/', treat as path-like: use basename.
    std::string base;
    if (pathOrName.find('/') != std::string::npos) {
        base = fs::path(pathOrName).filename().string();
    } else {
        base = pathOrName;
    }
    if (base.empty()) base = "lfc.telemetry";

    // POSIX SHM names must start with one leading slash and contain no others.
    // Replace any additional slashes just in case.
    for (char& c : base) {
        if (c == '/') c = '_';
    }
    if (base.front() != '/') base.insert(base.begin(), '/');
    return base;
}

// ------------------------------- ctor/dtor -----------------------------------

ShmTelemetry::ShmTelemetry(const std::string& pathOrName)
    : shmName_(toShmName(pathOrName))
{
    // If caller passed a path (contained '/'), keep it as file fallback.
    if (pathOrName.find('/') != std::string::npos) {
        fallbackPath_ = pathOrName;
    } else {
        // Provide a sane default fallback under /dev/shm if only a name was given.
        fallbackPath_ = std::string("/dev/shm/") + shmName_.substr(1);
    }
    LOG_DEBUG("telemetry: shmName=%s fallback=%s", shmName_.c_str(), fallbackPath_.c_str());
}

ShmTelemetry::~ShmTelemetry() = default;

// ------------------------------- JSON build ----------------------------------

nlohmann::json ShmTelemetry::buildJson(const HwmonInventory& hw,
                                       const std::vector<GpuSample>& gpus,
                                       const Profile& profile,
                                       bool engineEnabled)
{
    using nlohmann::json;
    json j;

    // Hwmon
    {
        json jchips = json::array();
        for (const auto& c : hw.chips) {
            jchips.push_back({{"path", c.hwmonPath}, {"name", c.name}, {"vendor", c.vendor}});
        }
        json jtemps = json::array();
        for (const auto& t : hw.temps) {
            jtemps.push_back({
                {"chip", t.chipPath},
                {"path", t.path_input},
                {"label", t.label},
            });
        }
        json jfans = json::array();
        for (const auto& f : hw.fans) {
            jfans.push_back({
                {"chip", f.chipPath},
                {"path", f.path_input},
                {"label", f.label},
            });
        }
        json jpwms = json::array();
        for (const auto& p : hw.pwms) {
            jpwms.push_back({
                {"chip", p.chipPath},
                {"pwm",  p.path_pwm},
                {"enable", p.path_enable},
                {"max",  p.pwm_max},
                {"label", p.label},
            });
        }

        j["hwmon"] = {
            {"chips", jchips},
            {"temps", jtemps},
            {"fans",  jfans},
            {"pwms",  jpwms}
        };
    }

    // GPUs
    {
        nlohmann::json jg = nlohmann::json::array();
        for (const auto& g : gpus) {
            jg.push_back({
                {"vendor",  g.vendor},
                {"name",    g.name},
                {"hwmon",   g.hwmonPath},
                {"pci",     g.pciBusId},
                {"index",   g.index},
                {"tempC",   g.tempC},
                {"utilPct", g.utilPct},
                {"memPct",  g.memPct},
                {"fanPct",  g.fanPct},
                {"fanRpm",  g.fanRpm},
                {"powerW",  g.powerW}
            });
        }
        j["gpus"] = std::move(jg);
    }

    // Profile summary (keep it lean; avoid dumping full internals unless needed)
    {
        nlohmann::json jp;
        jp["name"] = profile.name;
        j["profile"] = std::move(jp);
    }

    j["engineEnabled"] = engineEnabled;
    j["version"] = LFCD_VERSION; // schema/version tag for consumers

    return j;
}

// ------------------------------- publish -------------------------------------

bool ShmTelemetry::publish(const HwmonInventory& hw,
                           const std::vector<GpuSample>& gpus,
                           const Profile& profile,
                           bool engineEnabled,
                           const void* /*reserved*/)
{
    const auto j = buildJson(hw, gpus, profile, engineEnabled);
    const std::string text = j.dump(2);

    if (publishToShm(text)) {
        return true;
    }

    // Fallback to regular file
    return publishToFile(text);
}

bool ShmTelemetry::publishToShm(const std::string& jsonText) {
    const size_t size = jsonText.size() + 1; // include null terminator

    // O_CREAT | O_RDWR, user read/write only
    int fd = ::shm_open(shmName_.c_str(), O_CREAT | O_RDWR, 0600);
    if (fd < 0) {
        LOG_WARN("telemetry: shm_open('%s') failed: %s", shmName_.c_str(), std::strerror(errno));
        return false;
    }

    // Resize to fit content
    if (::ftruncate(fd, (off_t)size) != 0) {
        LOG_WARN("telemetry: ftruncate failed: %s", std::strerror(errno));
        ::close(fd);
        return false;
    }

    void* addr = ::mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (addr == MAP_FAILED) {
        LOG_WARN("telemetry: mmap failed: %s", std::strerror(errno));
        ::close(fd);
        return false;
    }

    // Write the JSON text (null-terminated) into SHM
    std::memcpy(addr, jsonText.c_str(), jsonText.size());
    static_cast<char*>(addr)[jsonText.size()] = '\0';

    // Ensure visibility to other processes
    if (::msync(addr, size, MS_SYNC) != 0) {
        LOG_DEBUG("telemetry: msync warning: %s", std::strerror(errno));
    }

    ::munmap(addr, size);
    ::close(fd);
    return true;
}

bool ShmTelemetry::publishToFile(const std::string& jsonText) {
    ensure_dir_for_file(fallbackPath_);
    std::ofstream f(fallbackPath_, std::ios::binary | std::ios::trunc);
    if (!f) {
        LOG_ERROR("telemetry: fallback file open failed: %s", fallbackPath_.c_str());
        return false;
    }
    f.write(jsonText.data(), (std::streamsize)jsonText.size());
    f.put('\n');
    const bool ok = (bool)f;
    if (!ok) {
        LOG_ERROR("telemetry: fallback file write failed: %s", fallbackPath_.c_str());
    }
    return ok;
}

} // namespace lfc
