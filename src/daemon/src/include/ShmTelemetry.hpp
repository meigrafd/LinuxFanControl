/*
 * Linux Fan Control — Telemetry (POSIX shared memory)
 * (c) 2025 LinuxFanControl contributors
 */
#pragma once

#include <string>
#include <vector>
#include <optional>
#include <nlohmann/json.hpp>

#include "include/Hwmon.hpp"
#include "include/GpuMonitor.hpp"
#include "include/Profile.hpp"

namespace lfc {

/**
 * Writes daemon telemetry as JSON into a POSIX shared memory object.
 * If shm_open() is unavailable or fails, falls back to writing a normal file
 * at the provided path.
 *
 * Naming:
 *  - If the provided string contains any '/', we treat it as a "path-like"
 *    and use only its basename to form a POSIX SHM name: "/<basename>".
 *  - If it contains no '/', we treat it as a bare SHM name and ensure it
 *    starts with a single leading slash.
 *
 * Examples:
 *   "/dev/shm/lfc.telemetry"   -> SHM name "/lfc.telemetry"
 *   "/run/user/1000/lfc.telemetry" -> SHM name "/lfc.telemetry"
 *   "lfc.telemetry"            -> SHM name "/lfc.telemetry"
 */
class ShmTelemetry {
public:
    explicit ShmTelemetry(const std::string& pathOrName);
    ~ShmTelemetry();

    // Build a JSON document representing the current state.
    static nlohmann::json buildJson(const HwmonInventory& hw,
                                    const std::vector<GpuSample>& gpus,
                                    const Profile& profile,
                                    bool engineEnabled);

    /**
     * Publish the given state to POSIX SHM.
     * Returns true on success. If POSIX SHM fails and file fallback is used,
     * also returns true when the file write succeeds.
     */
    bool publish(const HwmonInventory& hw,
                 const std::vector<GpuSample>& gpus,
                 const Profile& profile,
                 bool engineEnabled,
                 const void* /*reserved*/ = nullptr);

private:
    // Normalize to a valid POSIX SHM name like "/lfc.telemetry".
    static std::string toShmName(const std::string& pathOrName);

    // Try to publish into SHM. Returns true on success.
    bool publishToShm(const std::string& jsonText);

    // Fallback: publish to a regular file at fallbackPath_ (if set).
    bool publishToFile(const std::string& jsonText);

private:
    std::string shmName_;        // e.g. "/lfc.telemetry"
    std::string fallbackPath_;   // original pathOrName if it looked like a path
};

} // namespace lfc
