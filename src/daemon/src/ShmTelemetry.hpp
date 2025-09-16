/*
 * Linux Fan Control — SHM Telemetry
 * Writes a compact JSON blob to a file in tmpfs (/dev/shm by default)
 * and keeps the last JSON in memory for RPC "telemetry.json".
 * (c) 2025 LinuxFanControl contributors
 */
#pragma once
#include <string>
#include <mutex>

namespace lfc {

class ShmTelemetry {
public:
    ShmTelemetry() = default;

    // Initialize writer with target path (e.g. "/dev/shm/lfc_telemetry").
    // This does not create the file yet — it will be created on first update().
    bool init(const std::string& path);

    // Atomically replace telemetry content both on disk and in memory.
    // Returns true on success.
    bool update(const std::string& jsonPayload);

    // Retrieve the last in-memory JSON payload set by update().
    // Returns false if no payload was written yet.
    bool get(std::string& out) const;

    // Path accessor for diagnostics.
    std::string path() const;

private:
    std::string path_;
    mutable std::mutex mtx_;
    std::string last_;
    bool has_{false};

    bool writeAtomic(const std::string& p, const std::string& data);
};

} // namespace lfc
