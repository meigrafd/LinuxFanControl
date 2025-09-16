/*
 * Linux Fan Control — FanControl.Releases import (implementation)
 * - Minimal mapping to internal Profile
 * (c) 2025 LinuxFanControl contributors
 */
#include "FanControlImport.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <filesystem>

namespace lfc {

using nlohmann::json;

static bool readAll(const std::string& p, std::string& out) {
    std::ifstream f(p);
    if (!f) return false;
    out.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
    return true;
}

bool FanControlImport::LoadAndMap(const std::string& path, const HwmonSnapshot& /*snap*/, Profile& out, std::string& err) {
    std::string txt;
    if (!readAll(path, txt)) { err = "open failed"; return false; }
    json j;
    try {
        j = json::parse(txt);
    } catch (const std::exception& e) {
        err = std::string("parse failed: ") + e.what();
        return false;
    }

    out.name = std::filesystem::path(path).filename().string();
    out.items.clear();

    if (j.contains("Fans") && j["Fans"].is_array()) {
        for (auto& it : j["Fans"]) {
            Profile::Item pi;
            pi.pwmHint = it.value("Name", "");
            if (it.contains("Curve") && it["Curve"].is_array()) {
                for (auto& p : it["Curve"]) {
                    Profile::Point pt;
                    pt.mC = p.value("X", 0);
                    pt.percent = p.value("Y", 0);
                    pi.curve.push_back(pt);
                }
            }
            out.items.push_back(std::move(pi));
        }
    }

    return true;
}

} // namespace lfc
