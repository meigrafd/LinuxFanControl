/*
 * Linux Fan Control — SHM Telemetry (implementation)
 * - Simple mmap-backed JSON line buffer
 * (c) 2025 LinuxFanControl contributors
 */
#include "ShmTelemetry.hpp"

#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <filesystem>
#include <cstring>
#include <string>

namespace lfc {

ShmTelemetry::~ShmTelemetry() {
    close();
}

bool ShmTelemetry::openOrCreate(const std::string& path, std::size_t bytes) {
    return mapFile(path, bytes);
}

void ShmTelemetry::close() {
    if (ptr_) {
        ::msync(ptr_, cap_, MS_ASYNC);
        ::munmap(ptr_, cap_);
        ptr_ = nullptr;
    }
    if (fd_ != -1) {
        ::close(fd_);
        fd_ = -1;
    }
    path_.clear();
    cap_ = 0;
}

bool ShmTelemetry::appendJsonLine(const std::string& jsonLine) {
    if (!ptr_ || cap_ == 0) return false;
    std::size_t n = jsonLine.size();
    if (n + 2 >= cap_) n = cap_ - 2; // leave room for '\n' and '\0'
    std::memcpy(ptr_, jsonLine.data(), n);
    ptr_[n] = '\n';
    ptr_[n + 1] = '\0';
    return true;
}

bool ShmTelemetry::readAll(std::string& out) const {
    if (!ptr_ || cap_ == 0) return false;
    std::size_t i = 0;
    while (i < cap_ && ptr_[i] != '\0') ++i;
    out.assign(ptr_, i);
    return true;
}

bool ShmTelemetry::mapFile(const std::string& path, std::size_t bytes) {
    close();

    std::error_code ec;
    std::filesystem::create_directories(std::filesystem::path(path).parent_path(), ec);

    int fd = ::open(path.c_str(), O_RDWR | O_CREAT, 0660);
    if (fd < 0) return false;

    if (::ftruncate(fd, static_cast<off_t>(bytes)) != 0) {
        ::close(fd);
        return false;
    }

    void* p = ::mmap(nullptr, bytes, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (p == MAP_FAILED) {
        ::close(fd);
        return false;
    }

    path_ = path;
    fd_ = fd;
    ptr_ = static_cast<char*>(p);
    cap_ = bytes;

    std::memset(ptr_, 0, cap_);
    return true;
}

} // namespace lfc
