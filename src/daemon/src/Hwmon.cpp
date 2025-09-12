#include "Hwmon.h"
#include <filesystem>
#include <fstream>
#include <regex>

using std::string;
namespace fs = std::filesystem;

static inline bool starts_with(const string& s, const char* pfx) {
    return s.compare(0, std::char_traits<char>::length(pfx), pfx) == 0;
}
static inline bool ends_with(const string& s, const char* sfx) {
    size_t n = std::char_traits<char>::length(sfx);
    return s.size()>=n && s.compare(s.size()-n, n, sfx)==0;
}

string Hwmon::readFile(const string& path) {
    std::ifstream f(path);
    if (!f) return {};
    std::string s; std::getline(f, s);
    if (!s.empty() && s.back()=='\n') s.pop_back();
    return s;
}

bool Hwmon::isDir(const string& p) { std::error_code ec; return fs::is_directory(p, ec); }
string Hwmon::basename(const string& p) { return fs::path(p).filename().string(); }

string Hwmon::classify(const string& devName, const string& label) {
    auto low = [](string s){ for (auto& c: s) c= char(::tolower(c)); return s; };
    string n = low(devName), l = low(label);
    if (n.find("amdgpu") != string::npos || l.find("gpu")!=string::npos || l.find("hotspot")!=string::npos) return "GPU";
    if (n.find("k10temp")!=string::npos || n.find("coretemp")!=string::npos || l.find("cpu")!=string::npos || l.find("tctl")!=string::npos) return "CPU";
    if (l.find("water")!=string::npos || l.find("coolant")!=string::npos) return "Water";
    if (l.find("ambient")!=string::npos || l.find("room")!=string::npos) return "Ambient";
    if (l.find("nvme")!=string::npos) return "NVMe";
    if (l.find("vrm")!=string::npos || l.find("mos")!=string::npos) return "VRM";
    if (l.find("chipset")!=string::npos || l.find("pch")!=string::npos) return "Chipset";
    return "Unknown";
}

std::vector<TempSensorInfo> Hwmon::discoverTemps() const {
    std::vector<TempSensorInfo> out;
    const string base = "/sys/class/hwmon";
    if (!isDir(base)) return out;

    for (auto& e : fs::directory_iterator(base)) {
        if (!e.is_directory()) continue;
        string hw = e.path().string();
        string devname = readFile(hw + "/name");
        string hwid = basename(hw);
        // collect temp labels
        std::unordered_map<string, string> labelByTemp;
        for (auto& f : fs::directory_iterator(hw)) {
            string bn = basename(f.path().string());
            if (starts_with(bn, "temp") && ends_with(bn, "_label")) {
                string key = bn.substr(0, bn.size()-6); // strip _label
                labelByTemp[key] = readFile(f.path().string());
            }
        }
        for (auto& f : fs::directory_iterator(hw)) {
            string bn = basename(f.path().string());
            if (starts_with(bn, "temp") && ends_with(bn, "_input")) {
                string key = bn.substr(0, bn.size()-6); // strip _input
                string rawLabel = labelByTemp.count(key) ? labelByTemp[key] : key;
                string type = classify(devname, rawLabel);
                TempSensorInfo ts;
                ts.name = hwid + ":" + devname + ":" + rawLabel;
                ts.path = f.path().string();
                ts.type = type;
                out.push_back(std::move(ts));
            }
        }
    }
    return out;
}

std::vector<PwmOutputInfo> Hwmon::discoverPwms() const {
    std::vector<PwmOutputInfo> out;
    const string base = "/sys/class/hwmon";
    if (!isDir(base)) return out;

    std::regex rpwm("^pwm(\\d+)$");
    for (auto& e : fs::directory_iterator(base)) {
        if (!e.is_directory()) continue;
        string hw = e.path().string();
        string devname = readFile(hw + "/name");
        string hwid = basename(hw);

        for (auto& f : fs::directory_iterator(hw)) {
            string bn = basename(f.path().string());
            std::smatch m;
            if (std::regex_match(bn, m, rpwm)) {
                string n = m[1].str();
                string en = hw + "/pwm" + n + "_enable";
                string fan = hw + "/fan" + n + "_input";
                PwmOutputInfo po;
                po.label = hwid + ":" + devname + ":pwm" + n;
                po.pwmPath = f.path().string();
                po.enablePath = fs::exists(en) ? en : "";
                po.tachPath = fs::exists(fan) ? fan : "";
                out.push_back(std::move(po));
            }
        }
    }
    return out;
}
