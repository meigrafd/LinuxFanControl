/*
 * Linux Fan Control (lfcd) - Daemon implementation
 * (c) 2025 meigrafd & contributors - MIT License
 *
 * This file intentionally does NOT contain a test main().
 * The only main() lives in src/daemon/src/main.cpp.
 *
 * Notes:
 *  - Provides minimal JSON-RPC handlers so the GUI does not hang:
 *      - listSensors: enumerate /sys/class/hwmon temps
 *      - listPwms   : enumerate /sys/class/hwmon pwm/fan inputs
 *      - listChannels: returns empty array for now (placeholder)
 *      - detectCalibrate: returns a quick structured result (no heavy logic yet)
 *  - Heavy detection/calibration can be added incrementally.
 */

#include "Daemon.h"
#include "RpcServer.h"

#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <regex>
#include <string>
#include <vector>
#include <iostream>

namespace fs = std::filesystem;

// ---------- helpers ----------
static std::string slurp(const fs::path& p) {
    try {
        std::ifstream f(p);
        if (!f) return {};
        std::string s; std::getline(f, s);
        // trim
        while (!s.empty() && (s.back() == '\n' || s.back() == '\r')) s.pop_back();
        return s;
    } catch (...) { return {}; }
}

static bool is_number_str(const std::string& s) {
    if (s.empty()) return false;
    for (char c : s) if (c < '0' || c > '9') return false;
    return true;
}

static double milli_to_c(double v) {
    return (v > 200.0) ? (v / 1000.0) : v;
}

// hwmon enumeration compatible with our earlier Python logic (simplified)
static nlohmann::json enumerate_sensors_hwmon() {
    nlohmann::json arr = nlohmann::json::array();
    const fs::path base{"/sys/class/hwmon"};
    if (!fs::exists(base)) return arr;

    for (auto& dir : fs::directory_iterator(base)) {
        if (!dir.is_directory()) continue;
        fs::path hw = dir.path();
        std::string hwid = hw.filename().string();
        std::string name = slurp(hw / "name");
        if (name.empty()) name = hwid;

        // collect temp labels
        std::map<std::string, std::string> labels;
        for (auto& f : fs::directory_iterator(hw)) {
            const std::string fn = f.path().filename().string();
            if (fn.rfind("temp", 0) == 0 && fn.size() > 6 && fn.substr(fn.size()-6) == "_label") {
                const std::string key = fn.substr(0, fn.size()-6); // tempN
                labels[key] = slurp(f.path());
            }
        }
        for (auto& f : fs::directory_iterator(hw)) {
            const std::string fn = f.path().filename().string();
            if (fn.rfind("temp", 0) == 0 && fn.size() > 6 && fn.substr(fn.size()-6) == "_input") {
                const std::string tempn = fn.substr(0, fn.size()-6); // tempN
                const std::string raw_label = labels.count(tempn) ? labels[tempn] : tempn;
                const std::string nice = raw_label.empty() ? tempn : raw_label;
                nlohmann::json j;
                j["device"] = hwid + ":" + name;
                j["label"]  = j["device"].get<std::string>() + ":" + nice;
                j["raw_label"] = raw_label;
                j["type"]   = "Unknown"; // TODO: add classification hints (CPU/GPU/etc.)
                j["path"]   = f.path().string();
                j["unit"]   = u8"°C";
                arr.push_back(std::move(j));
            }
        }
    }
    return arr;
}

static nlohmann::json enumerate_pwms_hwmon() {
    nlohmann::json arr = nlohmann::json::array();
    const fs::path base{"/sys/class/hwmon"};
    if (!fs::exists(base)) return arr;

    for (auto& dir : fs::directory_iterator(base)) {
        if (!dir.is_directory()) continue;
        fs::path hw = dir.path();
        std::string hwid = hw.filename().string();
        std::string name = slurp(hw / "name");
        if (name.empty()) name = hwid;

        // locate pwmN, pwmN_enable, fanN_input
        std::map<std::string, fs::path> map_entries;
        for (auto& f : fs::directory_iterator(hw)) {
            const std::string fn = f.path().filename().string();
            map_entries[fn] = f.path();
        }
        for (const auto& kv : map_entries) {
            const std::string& fn = kv.first;
            if (fn.rfind("pwm", 0) == 0) {
                // accept "pwmN" (no suffix, N numeric)
                const std::string rest = fn.substr(3);
                if (!rest.empty() && is_number_str(rest)) {
                    const std::string n = rest;
                    nlohmann::json j;
                    j["device"] = hwid + ":" + name;
                    j["label"]  = j["device"].get<std::string>() + ":pwm" + n;
                    j["pwm_path"]    = (hw / ("pwm" + n)).string();
                    const auto enIt  = map_entries.find("pwm" + n + "_enable");
                    const auto tachIt= map_entries.find("fan" + n + "_input");
                    j["enable_path"] = (enIt != map_entries.end()) ? enIt->second.string() : "";
                    j["fan_input_path"] = (tachIt != map_entries.end()) ? tachIt->second.string() : "";
                    arr.push_back(std::move(j));
                }
            }
        }
    }
    return arr;
}

// ---------- Daemon ----------
Daemon::Daemon(bool debug)
: debug_(debug) {
    // Keep construction light; no I/O here.
}

Daemon::~Daemon() = default;

bool Daemon::init() {
    if (debug_) {
        std::cerr << "[daemon] init()\n";
    }

    // Register minimal JSON-RPC handlers so GUI doesn't hang.
    server_.on("listSensors", [this](const nlohmann::json& params) -> nlohmann::json {
        (void)params;
        if (debug_) std::cerr << "[rpc] listSensors\n";
        return enumerate_sensors_hwmon();
    });

    server_.on("listPwms", [this](const nlohmann::json& params) -> nlohmann::json {
        (void)params;
        if (debug_) std::cerr << "[rpc] listPwms\n";
        return enumerate_pwms_hwmon();
    });

    server_.on("listChannels", [this](const nlohmann::json& params) -> nlohmann::json {
        (void)params;
        if (debug_) std::cerr << "[rpc] listChannels\n";
        // Placeholder: return empty for now
        return nlohmann::json::array();
    });

    server_.on("detectCalibrate", [this](const nlohmann::json& params) -> nlohmann::json {
        (void)params;
        if (debug_) std::cerr << "[rpc] detectCalibrate (quick stub)\n";

        // Quick stub: return current snapshot of sensors/pwms and empty results.
        // (Heavy-weight detection/calibration can be integrated stepwise.)
        nlohmann::json out;
        out["sensors"]     = enumerate_sensors_hwmon();
        out["pwms"]        = enumerate_pwms_hwmon();
        out["mapping"]     = nlohmann::json::array(); // to be filled by active-coupling later
        out["calibration"] = nlohmann::json::array(); // to be filled by calibrator later
        out["skipped"]     = nlohmann::json::array();
        out["errors"]      = nlohmann::json::array();
        return out;
    });

    return true;
}

int Daemon::run() {
    if (debug_) {
        std::cerr << "[daemon] run() -> entering RPC loop\n";
    }
    // Blocks reading JSON lines from stdin and writing responses to stdout.
    return server_.run();
}

void Daemon::requestStop() {
    if (debug_) {
        std::cerr << "[daemon] requestStop()\n";
    }
    // Add server stop logic here if/when available, e.g. server_.stop();
}
