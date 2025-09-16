/*
 * Linux Fan Control — Config (header)
 * - Daemon config (nlohmann/json)
 * - Profile I/O compatible with FanControl.Releases schema
 * (c) 2025 LinuxFanControl contributors
 */
#pragma once
#include <string>
#include <cstddef>
#include <vector>

namespace lfc {

// ---------- Runtime (daemon) config ----------

struct DaemonConfig {
    struct Log {
        std::string file{"/tmp/daemon_lfc.log"};
        std::size_t maxBytes{5 * 1024 * 1024};
        int rotateCount{3};
        bool debug{false};
    } log;

    struct Rpc {
        std::string host{"127.0.0.1"};
        int port{8777};
    } rpc;

    struct Shm {
        std::string path{"/dev/shm/lfc_telemetry"};
    } shm;

    struct Profiles {
        // Will be set at runtime to: $XDG_CONFIG_HOME/LinuxFanControl/profiles or ~/.config/LinuxFanControl/profiles
        std::string dir;
        std::string active{"Default.json"};
        bool backups{true};
    } profiles;

    // Primary PID path; code falls back to /tmp and persists it if /run is not writable
    std::string pidFile{"/run/lfcd.pid"};
};

struct Config {
    static DaemonConfig Defaults();
    static bool Load(const std::string& path, DaemonConfig& out, std::string& err);
    static bool Save(const std::string& path, const DaemonConfig& in, std::string& err);
};

// ---------- Profile schema (FanControl.Releases compatible) ----------

struct Profile {
    struct ResponseTimeCfg {
        int ResponseTimeUp{3};
        int ResponseTimeDown{3};
    };
    struct HystCfg {
        int ResponseTimeUp{3};
        int ResponseTimeDown{3};
        int HysteresisValueUp{2};
        int HysteresisValueDown{2};
        bool IgnoreHysteresisAtLimits{true};
    };
    struct TempSource {
        std::string Identifier;
    };
    struct MixRef {
        std::string Name;
    };

    struct Curve {
        int CommandMode{0};
        std::string Name;
        bool IsHidden{false};
        std::vector<std::string> Points;
        TempSource SelectedTempSource;
        int MaximumTemperature{120};
        int MinimumTemperature{20};
        int MaximumCommand{100};
        HystCfg HysteresisConfig{};
        std::vector<MixRef> SelectedFanCurves;
        int SelectedMixFunction{0};
        TempSource TriggerTempSource;
        int LoadFanSpeed{80};
        int LoadTemperature{65};
        int IdleFanSpeed{30};
        int IdleTemperature{50};
        ResponseTimeCfg ResponseTimeConfig{};
    };

    struct Control {
        std::string Name;
        std::string NickName;
        std::string Identifier;
        bool IsHidden{false};
        bool Enable{true};
        struct RefName { std::string Name; };
        RefName SelectedFanCurve{};
        int SelectedOffset{0};
        std::string PairedFanSensor;
        int SelectedStart{0};
        int SelectedStop{0};
        int MinimumPercent{0};
        int SelectedCommandStepUp{8};
        int SelectedCommandStepDown{8};
        int ManualControlValue{0};
        bool ManualControl{false};
        bool ForceApply{false};
        std::string Calibration;
    };

    std::string Version{"236"};
    std::vector<Control> Controls;
    std::vector<Curve> FanCurves;

    struct FanSensorDesc { std::string Identifier; bool IsHidden{false}; std::string Name; std::string NickName; };
    std::vector<FanSensorDesc> FanSensors;

    bool HideCalibration{false};
    bool HideFanSpeedCards{false};
    bool HorizontalUIOrientation{false};
    bool Fahrenheit{false};
};

struct ProfileIO {
    static bool Load(const std::string& path, Profile& out, std::string& err);
    static bool Save(const std::string& path, const Profile& p, std::string& err);
    static bool Validate(const Profile& p, const struct HwmonSnapshot& snap, std::string& err);
};

} // namespace lfc
