#pragma once
/*
 * Linux Fan Control — Telemetry (interface)
 * Exposes a JSON snapshot of engine/gpu/hwmon state via POSIX shared memory.
 */

#include <memory>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace lfc {

struct HwmonInventory;
struct GpuSample;
struct Profile;

class ShmTelemetryImpl;

/**
 * Publishes telemetry as UTF-8 JSON to a POSIX SHM object (with file fallback).
 * Thread-safe on the call-site level if used from a single publisher thread.
 */
class ShmTelemetry {
public:
    // Accepts either a POSIX shm name ("/lfc.telemetry" or "lfc.telemetry") or an absolute file path.
    explicit ShmTelemetry(const std::string& shmNameOrPath);

    // Explicit shm name + explicit fallback file path (both can be empty to use defaults).
    ShmTelemetry(const std::string& shmName, const std::string& fallbackPath);

    ~ShmTelemetry();

    // Legacy API: publishes only hwmon inventory (for older call sites).
    bool publishSnapshot(const HwmonInventory& inv);
    bool publishSnapshot(const HwmonInventory& inv, nlohmann::json* detailsOut);

    // Full API: publishes hwmon, gpus, profile, and engine state.
    bool publishSnapshot(const HwmonInventory& inv,
                         const std::vector<GpuSample>& gpus,
                         const Profile& profile,
                         bool engineEnabled);
    bool publishSnapshot(const HwmonInventory& inv,
                         const std::vector<GpuSample>& gpus,
                         const Profile& profile,
                         bool engineEnabled,
                         nlohmann::json* detailsOut);

    // Alias names to stay compatible with Daemon.cpp (which calls telemetry_->publish(...)).
    bool publish(const HwmonInventory& inv) {
        return publishSnapshot(inv);
    }
    bool publish(const HwmonInventory& inv, nlohmann::json* detailsOut) {
        return publishSnapshot(inv, detailsOut);
    }
    bool publish(const HwmonInventory& inv,
                 const std::vector<GpuSample>& gpus,
                 const Profile& profile,
                 bool engineEnabled) {
        return publishSnapshot(inv, gpus, profile, engineEnabled);
    }
    bool publish(const HwmonInventory& inv,
                 const std::vector<GpuSample>& gpus,
                 const Profile& profile,
                 bool engineEnabled,
                 nlohmann::json* detailsOut) {
        return publishSnapshot(inv, gpus, profile, engineEnabled, detailsOut);
    }

    // Builds the JSON document (exposed for tests).
    static nlohmann::json buildJson(const HwmonInventory& hw,
                                    const std::vector<GpuSample>& gpus,
                                    const Profile& profile,
                                    bool engineEnabled);

private:
    std::unique_ptr<ShmTelemetryImpl> impl_;
};

} // namespace lfc
