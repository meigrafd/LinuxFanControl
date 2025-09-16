/*
 * Linux Fan Control — SHM Telemetry
 * (c) 2025 LinuxFanControl contributors
 */
#include "ShmTelemetry.hpp"
#include <fstream>
#include <string>
#include <cstdio>
#include <cerrno>
#include <cstring>

namespace lfc {

bool ShmTelemetry::init(const std::string& path) {
    std::lock_guard<std::mutex> lk(mtx_);
    path_ = path;
    return !path_.empty();
}

bool ShmTelemetry::writeAtomic(const std::string& p, const std::string& data) {
    // Write to <path>.tmp and then rename() to <path> for atomic swap on POSIX
    std::string tmp = p + ".tmp";
    {
        std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
        if (!f) return false;
        f.write(data.data(), static_cast<std::streamsize>(data.size()));
        if (!f.good()) return false;
    }
    if (std::rename(tmp.c_str(), p.c_str()) != 0) {
        // Cleanup tmp on failure
        std::remove(tmp.c_str());
        return false;
    }
    return true;
}

bool ShmTelemetry::update(const std::string& jsonPayload) {
    std::lock_guard<std::mutex> lk(mtx_);
    if (path_.empty()) return false;
    if (!writeAtomic(path_, jsonPayload)) return false;
    last_ = jsonPayload;
    has_ = true;
    return true;
}

bool ShmTelemetry::get(std::string& out) const {
    std::lock_guard<std::mutex> lk(mtx_);
    if (!has_) return false;
    out = last_;
    return true;
}

std::string ShmTelemetry::path() const {
    std::lock_guard<std::mutex> lk(mtx_);
    return path_;
}

} // namespace lfc
