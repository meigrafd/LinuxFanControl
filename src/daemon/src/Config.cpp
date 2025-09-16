/*
 * Linux Fan Control — Config (implementation)
 * - Daemon config (nlohmann/json)
 * - Profile I/O compatible with FanControl.Releases schema
 * (c) 2025 LinuxFanControl contributors
 */
#include "Config.hpp"
#include "Hwmon.hpp"

#include <nlohmann/json.hpp>
#include <fstream>
#include <sstream>
#include <filesystem>

using nlohmann::json;

namespace lfc {

// ---------- Daemon config ----------

DaemonConfig Config::Defaults() {
    return DaemonConfig{};
}

static bool read_file(const std::string& path, std::string& out, std::string& err) {
    if (!std::filesystem::exists(path)) { err = "config not found"; return false; }
    std::ifstream f(path);
    if (!f) { err = "open failed"; return false; }
    std::ostringstream ss; ss << f.rdbuf();
    out = ss.str();
    return true;
}

bool Config::Load(const std::string& path, DaemonConfig& out, std::string& err) {
    std::string txt;
    if (!read_file(path, txt, err)) return false;

    json j;
    try { j = json::parse(txt); }
    catch (const std::exception& e) { err = std::string("parse failed: ") + e.what(); return false; }

    DaemonConfig cfg = Defaults();

    if (j.contains("log") && j["log"].is_object()) {
        const auto& jl = j["log"];
        cfg.log.file        = jl.value("file",        cfg.log.file);
        cfg.log.maxBytes    = static_cast<std::size_t>(jl.value("maxBytes",   static_cast<std::uint64_t>(cfg.log.maxBytes)));
        cfg.log.rotateCount = jl.value("rotateCount", cfg.log.rotateCount);
        cfg.log.debug       = jl.value("debug",       cfg.log.debug);
    }

    if (j.contains("rpc") && j["rpc"].is_object()) {
        const auto& jr = j["rpc"];
        cfg.rpc.host = jr.value("host", cfg.rpc.host);
        cfg.rpc.port = jr.value("port", cfg.rpc.port);
    }

    if (j.contains("shm") && j["shm"].is_object()) {
        const auto& js = j["shm"];
        cfg.shm.path = js.value("path", cfg.shm.path);
    }

    if (j.contains("profiles") && j["profiles"].is_object()) {
        const auto& jp = j["profiles"];
        cfg.profiles.dir     = jp.value("dir",     cfg.profiles.dir);
        cfg.profiles.active  = jp.value("active",  cfg.profiles.active);
        cfg.profiles.backups = jp.value("backups", cfg.profiles.backups);
    }

    cfg.pidFile = j.value("pidFile", cfg.pidFile);

    out = cfg;
    return true;
}

bool Config::Save(const std::string& path, const DaemonConfig& in, std::string& err) {
    std::error_code ec;
    std::filesystem::create_directories(std::filesystem::path(path).parent_path(), ec);

    json j;
    j["log"] = {
        {"file",        in.log.file},
        {"maxBytes",    static_cast<std::uint64_t>(in.log.maxBytes)},
        {"rotateCount", in.log.rotateCount},
        {"debug",       in.log.debug}
    };
    j["rpc"] = { {"host", in.rpc.host}, {"port", in.rpc.port} };
    j["shm"] = { {"path", in.shm.path} };
    j["profiles"] = {
        {"dir",     in.profiles.dir},
        {"active",  in.profiles.active},
        {"backups", in.profiles.backups}
    };
    j["pidFile"] = in.pidFile;

    std::ofstream f(path, std::ios::trunc);
    if (!f) { err = "open for write failed"; return false; }
    f << j.dump(4) << '\n';
    return static_cast<bool>(f);
}

// ---------- Profile I/O (FanControl.Releases compatible) ----------

static json profile_to_json(const Profile& p) {
    json main;

    // Controls
    main["Controls"] = json::array();
    for (const auto& c : p.Controls) {
        json jc;
        jc["Name"] = c.Name;
        jc["NickName"] = c.NickName;
        jc["Identifier"] = c.Identifier;
        jc["IsHidden"] = c.IsHidden;
        jc["Enable"] = c.Enable;
        if (!c.SelectedFanCurve.Name.empty())
            jc["SelectedFanCurve"] = json{ {"Name", c.SelectedFanCurve.Name} };
        else
            jc["SelectedFanCurve"] = nullptr;
        jc["SelectedOffset"] = c.SelectedOffset;
        if (!c.PairedFanSensor.empty()) jc["PairedFanSensor"] = c.PairedFanSensor; else jc["PairedFanSensor"] = nullptr;
        jc["SelectedStart"] = c.SelectedStart;
        jc["SelectedStop"] = c.SelectedStop;
        jc["MinimumPercent"] = c.MinimumPercent;
        jc["SelectedCommandStepUp"] = c.SelectedCommandStepUp;
        jc["SelectedCommandStepDown"] = c.SelectedCommandStepDown;
        jc["ManualControlValue"] = c.ManualControlValue;
        jc["ManualControl"] = c.ManualControl;
        jc["ForceApply"] = c.ForceApply;
        if (!c.Calibration.empty()) jc["Calibration"] = c.Calibration; else jc["Calibration"] = nullptr;
        main["Controls"].push_back(std::move(jc));
    }

    // FanCurves
    main["FanCurves"] = json::array();
    for (const auto& fc : p.FanCurves) {
        json jf;
        jf["CommandMode"] = fc.CommandMode;
        jf["Name"] = fc.Name;
        jf["IsHidden"] = fc.IsHidden;

        if (fc.CommandMode == 0) {
            jf["Points"] = fc.Points;
            if (!fc.SelectedTempSource.Identifier.empty())
                jf["SelectedTempSource"] = json{ {"Identifier", fc.SelectedTempSource.Identifier} };
            jf["MaximumTemperature"] = fc.MaximumTemperature;
            jf["MinimumTemperature"] = fc.MinimumTemperature;
            jf["MaximumCommand"] = fc.MaximumCommand;
            jf["HysteresisConfig"] = {
                {"ResponseTimeUp",     fc.HysteresisConfig.ResponseTimeUp},
                {"ResponseTimeDown",   fc.HysteresisConfig.ResponseTimeDown},
                {"HysteresisValueUp",  fc.HysteresisConfig.HysteresisValueUp},
                {"HysteresisValueDown",fc.HysteresisConfig.HysteresisValueDown},
                {"IgnoreHysteresisAtLimits", fc.HysteresisConfig.IgnoreHysteresisAtLimits}
            };
        } else {
            if (!fc.SelectedFanCurves.empty()) {
                jf["SelectedFanCurves"] = json::array();
                for (const auto& r : fc.SelectedFanCurves) jf["SelectedFanCurves"].push_back(json{{"Name", r.Name}});
            }
            jf["SelectedMixFunction"] = fc.SelectedMixFunction;

            if (!fc.TriggerTempSource.Identifier.empty())
                jf["SelectedTempSource"] = json{ {"Identifier", fc.TriggerTempSource.Identifier} };
            jf["LoadFanSpeed"]  = fc.LoadFanSpeed;
            jf["LoadTemperature"] = fc.LoadTemperature;
            jf["IdleFanSpeed"]  = fc.IdleFanSpeed;
            jf["IdleTemperature"] = fc.IdleTemperature;
            jf["ResponseTimeConfig"] = {
                {"ResponseTimeUp",   fc.ResponseTimeConfig.ResponseTimeUp},
                {"ResponseTimeDown", fc.ResponseTimeConfig.ResponseTimeDown}
            };
        }

        main["FanCurves"].push_back(std::move(jf));
    }

    // FanSensors (optional)
    if (!p.FanSensors.empty()) {
        main["FanSensors"] = json::array();
        for (const auto& fs : p.FanSensors) {
            main["FanSensors"].push_back({
                {"Identifier", fs.Identifier},
                {"IsHidden", fs.IsHidden},
                {"Name", fs.Name},
                {"NickName", fs.NickName}
            });
        }
    } else {
        main["FanSensors"] = json::array();
    }

    main["CustomSensors"] = json::array();
    main["Fahrenheit"] = p.Fahrenheit;
    main["HideCalibration"] = p.HideCalibration;
    main["HideFanSpeedCards"] = p.HideFanSpeedCards;
    main["HorizontalUIOrientation"] = p.HorizontalUIOrientation;

    json root;
    root["__VERSION__"] = p.Version;
    root["Main"] = std::move(main);
    root["Sensors"] = json::object(); // left minimal; can be expanded later
    return root;
}

static bool profile_from_json(const json& root, Profile& out) {
    if (!root.is_object() || !root.contains("Main")) return false;
    const auto& jmain = root.at("Main");
    Profile p;

    p.Version = root.value<std::string>("__VERSION__", p.Version);

    // Controls
    if (jmain.contains("Controls") && jmain["Controls"].is_array()) {
        for (const auto& jc : jmain["Controls"]) {
            Profile::Control c;
            c.Name = jc.value("Name", "");
            c.NickName = jc.value("NickName", "");
            c.Identifier = jc.value("Identifier", "");
            c.IsHidden = jc.value("IsHidden", false);
            c.Enable = jc.value("Enable", true);
            if (jc.contains("SelectedFanCurve") && jc["SelectedFanCurve"].is_object()) {
                c.SelectedFanCurve.Name = jc["SelectedFanCurve"].value("Name", "");
            }
            c.SelectedOffset = jc.value("SelectedOffset", 0);
            if (jc.contains("PairedFanSensor")) {
                if (jc["PairedFanSensor"].is_string()) c.PairedFanSensor = jc["PairedFanSensor"].get<std::string>();
            }
            c.SelectedStart = jc.value("SelectedStart", 0);
            c.SelectedStop  = jc.value("SelectedStop", 0);
            c.MinimumPercent = jc.value("MinimumPercent", 0);
            c.SelectedCommandStepUp = jc.value("SelectedCommandStepUp", 8);
            c.SelectedCommandStepDown = jc.value("SelectedCommandStepDown", 8);
            c.ManualControlValue = jc.value("ManualControlValue", 0);
            c.ManualControl = jc.value("ManualControl", false);
            c.ForceApply = jc.value("ForceApply", false);
            if (jc.contains("Calibration") && jc["Calibration"].is_string()) c.Calibration = jc["Calibration"].get<std::string>();
            p.Controls.push_back(std::move(c));
        }
    }

    // FanCurves
    if (jmain.contains("FanCurves") && jmain["FanCurves"].is_array()) {
        for (const auto& jf : jmain["FanCurves"]) {
            Profile::Curve fc;
            fc.CommandMode = jf.value("CommandMode", 0);
            fc.Name = jf.value("Name", "");
            fc.IsHidden = jf.value("IsHidden", false);

            if (fc.CommandMode == 0) {
                if (jf.contains("Points") && jf["Points"].is_array()) {
                    for (const auto& s : jf["Points"]) {
                        if (s.is_string()) fc.Points.push_back(s.get<std::string>());
                    }
                }
                if (jf.contains("SelectedTempSource") && jf["SelectedTempSource"].is_object()) {
                    fc.SelectedTempSource.Identifier = jf["SelectedTempSource"].value("Identifier", "");
                }
                fc.MaximumTemperature = jf.value("MaximumTemperature", 120);
                fc.MinimumTemperature = jf.value("MinimumTemperature", 20);
                fc.MaximumCommand = jf.value("MaximumCommand", 100);
                if (jf.contains("HysteresisConfig") && jf["HysteresisConfig"].is_object()) {
                    const auto& h = jf["HysteresisConfig"];
                    fc.HysteresisConfig.ResponseTimeUp = h.value("ResponseTimeUp", 3);
                    fc.HysteresisConfig.ResponseTimeDown = h.value("ResponseTimeDown", 3);
                    fc.HysteresisConfig.HysteresisValueUp = h.value("HysteresisValueUp", 2);
                    fc.HysteresisConfig.HysteresisValueDown = h.value("HysteresisValueDown", 2);
                    fc.HysteresisConfig.IgnoreHysteresisAtLimits = h.value("IgnoreHysteresisAtLimits", true);
                }
            } else {
                if (jf.contains("SelectedFanCurves") && jf["SelectedFanCurves"].is_array()) {
                    for (const auto& r : jf["SelectedFanCurves"]) {
                        Profile::MixRef mr; mr.Name = r.value("Name", "");
                        fc.SelectedFanCurves.push_back(std::move(mr));
                    }
                }
                fc.SelectedMixFunction = jf.value("SelectedMixFunction", 0);

                if (jf.contains("SelectedTempSource") && jf["SelectedTempSource"].is_object()) {
                    fc.TriggerTempSource.Identifier = jf["SelectedTempSource"].value("Identifier", "");
                }
                fc.LoadFanSpeed  = jf.value("LoadFanSpeed", 80);
                fc.LoadTemperature = jf.value("LoadTemperature", 65);
                fc.IdleFanSpeed  = jf.value("IdleFanSpeed", 30);
                fc.IdleTemperature = jf.value("IdleTemperature", 50);
                if (jf.contains("ResponseTimeConfig") && jf["ResponseTimeConfig"].is_object()) {
                    const auto& r = jf["ResponseTimeConfig"];
                    fc.ResponseTimeConfig.ResponseTimeUp = r.value("ResponseTimeUp", 5);
                    fc.ResponseTimeConfig.ResponseTimeDown = r.value("ResponseTimeDown", 5);
                }
            }

            p.FanCurves.push_back(std::move(fc));
        }
    }

    // FanSensors (optional)
    if (jmain.contains("FanSensors") && jmain["FanSensors"].is_array()) {
        for (const auto& js : jmain["FanSensors"]) {
            Profile::FanSensorDesc d;
            d.Identifier = js.value("Identifier", "");
            d.IsHidden   = js.value("IsHidden", false);
            d.Name       = js.value("Name", "");
            d.NickName   = js.value("NickName", "");
            p.FanSensors.push_back(std::move(d));
        }
    }

    p.Fahrenheit = jmain.value("Fahrenheit", false);
    p.HideCalibration = jmain.value("HideCalibration", false);
    p.HideFanSpeedCards = jmain.value("HideFanSpeedCards", false);
    p.HorizontalUIOrientation = jmain.value("HorizontalUIOrientation", false);

    out = std::move(p);
    return true;
}

bool ProfileIO::Load(const std::string& path, Profile& out, std::string& err) {
    if (!std::filesystem::exists(path)) { err = "profile not found"; return false; }
    std::ifstream f(path);
    if (!f) { err = "open failed"; return false; }
    json j;
    try { f >> j; }
    catch (const std::exception& e) { err = e.what(); return false; }
    return profile_from_json(j, out);
}

bool ProfileIO::Save(const std::string& path, const Profile& p, std::string& err) {
    std::error_code ec;
    std::filesystem::create_directories(std::filesystem::path(path).parent_path(), ec);
    std::ofstream f(path, std::ios::trunc);
    if (!f) { err = "open for write failed"; return false; }
    f << profile_to_json(p).dump(4) << '\n';
    return static_cast<bool>(f);
}

bool ProfileIO::Validate(const Profile& p, const HwmonSnapshot& /*snap*/, std::string& err) {
    if (p.Controls.empty()) { err = "no controls"; return false; }
    for (const auto& c : p.Controls) {
        if (c.Enable && c.SelectedFanCurve.Name.empty() && !c.ManualControl) {
            err = "control without curve"; return false;
        }
    }
    return true;
}

} // namespace lfc
