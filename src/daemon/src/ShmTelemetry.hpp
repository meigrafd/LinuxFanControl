/*
 * Linux Fan Control — SHM Telemetry (header)
 * - Simple mmap-backed JSON line buffer
 * (c) 2025 LinuxFanControl contributors
 */
#pragma once
#include <string>
#include <cstddef>

namespace lfc {

class ShmTelemetry {
public:
    ShmTelemetry() = default;
    ~ShmTelemetry();

    // Uses a regular file mmap (works with /dev/shm/* and any tmpfs/dir)
    bool openOrCreate(const std::string& path, std::size_t bytes);
    void close();

    // Writes one JSON line, replaces previous contents
    bool appendJsonLine(const std::string& jsonLine);

    // Optional readback (latest full buffer content)
    bool readAll(std::string& out) const;

private:
    bool mapFile(const std::string& path, std::size_t bytes);

private:
    std::string path_;
    int fd_{-1};
    char* ptr_{nullptr};
    std::size_t cap_{0};
};

} // namespace lfc
