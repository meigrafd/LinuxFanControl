/*
 * Linux Fan Control (LFC) - hwmon helpers
 * (c) 2025 meigrafd & contributors - MIT License
 */

#include "Hwmon.h"
#include <dirent.h>
#include <sys/stat.h>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>

static bool ends_with(const std::string& s, const char* suff) {
    const size_t ls = s.size();
    const size_t lt = std::char_traits<char>::length(suff);
    return (ls >= lt) && (s.compare(ls - lt, lt, suff) == 0);
}

static std::string read_all(const std::string& p) {
    std::ifstream f(p);
    if (!f.good()) return {};
    std::string s; std::getline(f, s);
    // trim
    s.erase(std::remove_if(s.begin(), s.end(), [](unsigned char c){ return c=='\r'||c=='\n'; }), s.end());
    return s;
}

std::vector<TempSensorInfo> Hwmon::discoverTemps() const {
    std::vector<TempSensorInfo> out;
    DIR* d = ::opendir("/sys/class/hwmon");
    if (!d) return out;
    while (auto* e = ::readdir(d)) {
        if (e->d_name[0]=='.') continue;
        std::string base = std::string("/sys/class/hwmon/") + e->d_name;
        std::string name = read_all(base + "/name");
        // labels
        std::vector<std::pair<std::string,std::string>> labels;
        DIR* d2 = ::opendir(base.c_str());
        if (!d2) continue;
        while (auto* e2 = ::readdir(d2)) {
            std::string fn = e2->d_name;
            if (fn.rfind("temp", 0)==0 && fn.size()>6 && ends_with(fn, "_label")) {
                std::string key = fn.substr(0, fn.size()-6);
                std::string lab = read_all(base + "/" + fn);
                labels.emplace_back(key, lab);
            }
        }
        ::closedir(d2);
        // inputs
        d2 = ::opendir(base.c_str());
        if (!d2) continue;
        while (auto* e2 = ::readdir(d2)) {
            std::string fn = e2->d_name;
            if (fn.rfind("temp", 0)==0 && fn.size()>6 && ends_with(fn, "_input")) {
                std::string key = fn.substr(0, fn.size()-6);
                std::string lab = key;
                auto it = std::find_if(labels.begin(), labels.end(), [&](auto& p){ return p.first==key; });
                if (it!=labels.end()) lab = it->second;
                TempSensorInfo info;
                info.device = e->d_name;
                info.name   = name;
                info.label  = lab;
                info.path   = base + "/" + fn;
                out.push_back(info);
            }
        }
        ::closedir(d2);
    }
    ::closedir(d);
    return out;
}
