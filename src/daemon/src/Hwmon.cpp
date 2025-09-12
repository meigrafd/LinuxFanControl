#include "Hwmon.h"
#include <filesystem>
#include <fstream>
#include <regex>
#include <unordered_map>
#include <charconv>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <algorithm>
#include <cmath> // std::lround

using namespace std;
namespace fs = std::filesystem;

// -------------------- discovery: temps --------------------
std::vector<TempSensorInfo> Hwmon::discoverTemps() const {
    std::vector<TempSensorInfo> out;
    const fs::path base("/sys/class/hwmon");
    if (!fs::exists(base)) return out;

    for (auto& d : fs::directory_iterator(base)) {
        if (!fs::is_directory(d)) continue;
        const fs::path dir = d.path();

        std::string devname;
        {
            std::ifstream f(dir / "name");
            if (f) std::getline(f, devname);
        }

        // tempN -> label (if *_label exists)
        std::unordered_map<std::string, std::string> labelByTemp;
        for (auto& e : fs::directory_iterator(dir)) {
            const std::string fn = e.path().filename().string();
            if (fn.rfind("temp", 0) == 0 && fn.size() > 6 && fn.ends_with("_label")) {
                std::ifstream fl(e.path());
                std::string lab;
                if (fl) std::getline(fl, lab);
                const auto n = fn.substr(0, fn.find('_')); // "tempN"
                labelByTemp[n] = lab;
            }
        }

        for (auto& e : fs::directory_iterator(dir)) {
            const std::string fn = e.path().filename().string();
            if (fn.rfind("temp", 0) == 0 && fn.size() > 6 && fn.ends_with("_input")) {
                const std::string n   = fn.substr(0, fn.find('_')); // tempN
                const std::string raw = (labelByTemp.count(n) ? labelByTemp[n] : n);

                // type guess
                std::string low = raw; std::transform(low.begin(), low.end(), low.begin(), ::tolower);
                std::string type = "Unknown";
                auto has = [&](const char* s){ return low.find(s) != std::string::npos; };
                if (has("cpu") || has("tctl") || has("tdie")) type = "CPU";
                else if (has("gpu") || has("hotspot") || has("junction")) type = "GPU";
                else if (has("nvme")) type = "NVMe";
                else if (has("ambient")) type = "Ambient";
                else if (has("water") || has("liquid")) type = "Water";

                TempSensorInfo info;
                info.name  = raw; // short
                info.label = devname.empty() ? (dir.filename().string()+":"+raw) : (devname+":"+raw);
                info.path  = e.path().string();
                info.type  = type;
                out.push_back(std::move(info));
            }
        }
    }
    return out;
}

// -------------------- discovery: pwms ---------------------
std::vector<PwmDevice> Hwmon::discoverPwms() const {
    std::vector<PwmDevice> out;
    const fs::path base("/sys/class/hwmon");
    if (!fs::exists(base)) return out;

    for (auto& d : fs::directory_iterator(base)) {
        if (!fs::is_directory(d)) continue;
        const fs::path dir = d.path();
        for (auto& e : fs::directory_iterator(dir)) {
            const std::string fn = e.path().filename().string();
            if (fn.rfind("pwm", 0) == 0 && fn.size() >= 4 && std::isdigit(static_cast<unsigned char>(fn[3]))) {
                PwmDevice dev;
                dev.pwm_path = e.path().string();
                const std::string n = fn.substr(3); // "N"
                const fs::path enable = dir / ("pwm"+n+"_enable");
                if (fs::exists(enable)) dev.enable_path = enable.string();
                const fs::path tach = dir / ("fan"+n+"_input");
                if (fs::exists(tach)) dev.tach_path = tach.string();
                out.push_back(std::move(dev));
            }
        }
    }
    return out;
}

// -------------------- helpers -----------------------------
double Hwmon::readTempC(const std::string& path) {
    std::ifstream f(path);
    if (!f) return std::numeric_limits<double>::quiet_NaN();
    long long raw = 0; f >> raw;
    return milli_to_c(static_cast<double>(raw));
}

bool Hwmon::setPwmPercent(const PwmDevice& dev, double pct, std::string* err) {
    const int v = std::clamp(static_cast<int>(std::lround(std::clamp(pct, 0.0, 100.0) * 255.0 / 100.0)), 0, 255);
    try {
        if (!dev.enable_path.empty()) {
            std::ofstream en(dev.enable_path);
            if (en) en << "1";
        }
        std::ofstream p(dev.pwm_path);
        if (!p) throw std::runtime_error("open pwm failed");
        p << v;
        return true;
    } catch (const std::exception& e) {
        if (err) *err = e.what();
        return false;
    }
}
