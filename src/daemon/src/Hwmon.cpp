#include "Hwmon.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <regex>
#include <cmath>

using std::string;
namespace fs = std::filesystem;

static double milli_to_c(double v) { return (v > 200.0 ? v/1000.0 : v); }
double Hwmon::clamp(double v, double lo, double hi) { return v<lo?lo:(v>hi?hi:v); }

bool Hwmon::readText(const string& path, string& out) {
    std::ifstream f(path);
    if (!f.is_open()) return false;
    std::ostringstream ss; ss << f.rdbuf();
    out = ss.str();
    if (!out.empty() && (out.back()=='\n' || out.back()=='\r')) out.pop_back();
    return true;
}
bool Hwmon::writeText(const string& path, const string& text) {
    std::ofstream f(path);
    if (!f.is_open()) return false;
    f << text;
    return (bool)f;
}

std::optional<double> Hwmon::readTempC(const string& path) {
    string s;
    if (!readText(path, s)) return std::nullopt;
    try {
        double v = std::stod(s);
        return milli_to_c(v);
    } catch (...) { return std::nullopt; }
}

std::optional<int> Hwmon::readRpm(const string& path) {
    if (path.empty()) return std::nullopt;
    string s;
    if (!readText(path, s)) return std::nullopt;
    try { return std::stoi(s); } catch (...) { return std::nullopt; }
}

static string read_name(const fs::path& dir) {
    string s;
    Hwmon::readText((dir/"name").string(), s);
    if (s.empty()) s = dir.filename().string();
    return s;
}

static bool is_file(const fs::path& p, const std::string& pref, const std::string& suf) {
    auto fn = p.filename().string();
    return fs::is_regular_file(p) && fn.starts_with(pref) && fn.size() > pref.size() && fn.ends_with(suf);
}

std::string Hwmon::classify(const std::string& name, const std::string& label) {
    auto norm = [](std::string s){ std::transform(s.begin(), s.end(), s.begin(), ::tolower); return s; };
    std::string n = norm(name), l = norm(label);
    auto has = [](const std::string& s, std::initializer_list<const char*> pats){
        for (auto p: pats) if (s.find(p) != std::string::npos) return true; return false;
    };
        if (has(n, {"k10temp","zenpower","coretemp"}) || has(l, {"cpu","tctl","package","tdie"})) return "CPU";
        if (has(n, {"amdgpu","nvidia"}) || has(l, {"gpu","hotspot","junction","edge"})) return "GPU";
        if (has(n, {"nvme"}) || has(l, {"nvme","ssd"})) return "NVMe";
        if (has(l, {"water","coolant","liquid"})) return "Water";
        if (has(l, {"ambient","room"})) return "Ambient";
        if (has(l, {"vrm","mos"})) return "VRM";
        return "Unknown";
}

std::vector<TempSensor> Hwmon::discoverTemps(const string& base) {
    std::vector<TempSensor> out;
    if (!fs::is_directory(base)) return out;
    for (auto& ent : fs::directory_iterator(base)) {
        if (!ent.is_directory()) continue;
        auto hw = ent.path();
        auto name = read_name(hw);
        // load tempX_label first
        std::unordered_map<string,string> labels;
        for (auto& f : fs::directory_iterator(hw)) {
            auto p = f.path();
            auto fn = p.filename().string();
            if (is_file(p, "temp", "_label")) {
                labels[fn.substr(0, fn.find('_'))] = [&](){ string s; readText(p.string(), s); return s; }();
            }
        }
        for (auto& f : fs::directory_iterator(hw)) {
            auto p = f.path();
            auto fn = p.filename().string();
            if (is_file(p, "temp", "_input")) {
                string key = fn.substr(0, fn.find('_'));
                string raw = labels.count(key) ? labels[key] : key;
                string typ = classify(name, raw);
                TempSensor t;
                t.label = hw.filename().string() + ":" + name + ":" + raw;
                t.path = p.string();
                t.type = typ;
                out.push_back(std::move(t));
            }
        }
    }
    return out;
}

std::vector<PwmDevice> Hwmon::discoverPwms(const string& base) {
    std::vector<PwmDevice> out;
    if (!fs::is_directory(base)) return out;
    for (auto& ent : fs::directory_iterator(base)) {
        if (!ent.is_directory()) continue;
        auto hw = ent.path();
        auto name = read_name(hw);
        // collect files
        std::unordered_map<int, string> pwm, enable, tach;
        for (auto& f : fs::directory_iterator(hw)) {
            auto p = f.path(); auto fn = p.filename().string();
            if (fn.rfind("pwm",0)==0 && fn.size()>3 && std::isdigit(fn[3]) && fn.find('_')==std::string::npos)
                pwm[std::stoi(fn.substr(3))] = p.string();
            if (fn.rfind("pwm",0)==0 && fn.ends_with("_enable")) {
                int n = std::stoi(fn.substr(3, fn.size()-3-7));
                enable[n] = p.string();
            }
            if (fn.rfind("fan",0)==0 && fn.ends_with("_input")) {
                int n = std::stoi(fn.substr(3, fn.size()-3-6));
                tach[n] = p.string();
            }
        }
        for (auto& [n, pp] : pwm) {
            PwmDevice d;
            d.label = hw.filename().string() + ":" + name + ":" + ("pwm"+std::to_string(n));
            d.pwm_path = pp;
            d.enable_path = enable.count(n) ? enable[n] : "";
            d.tach_path = tach.count(n) ? tach[n] : "";
            // writable?
            std::ofstream tryw(pp);
            d.writable = tryw.is_open();
            out.push_back(std::move(d));
        }
    }
    return out;
}

bool Hwmon::setPwmPercent(const PwmDevice& dev, double pct, std::string* err) {
    double cl = clamp(pct, 0.0, 100.0);
    int raw = int(std::round(cl * 255.0 / 100.0));
    if (!dev.enable_path.empty()) {
        // set manual if possible
        (void)writeText(dev.enable_path, "1");
    }
    if (!writeText(dev.pwm_path, std::to_string(raw))) {
        if (err) *err = "PWM write failed (EPERM/EOPNOTSUPP?)";
        return false;
    }
    return true;
}
