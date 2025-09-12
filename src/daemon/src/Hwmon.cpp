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
    // trim trailing newlines
    s.erase(std::remove_if(s.begin(), s.end(), [](unsigned char c){ return c=='\r'||c=='\n'; }), s.end());
    return s;
}

std::vector<TempSensorInfo> Hwmon::discoverTemps() const {
    std::vector<TempSensorInfo> out;
    DIR* d = ::opendir("/sys/class/hwmon");
    if (!d) return out;

    while (auto* e = ::readdir(d)) {
        if (!e || e->d_name[0]=='.') continue;
        const std::string base = std::string("/sys/class/hwmon/") + e->d_name;
        const std::string name = read_all(base + "/name");

        // collect labels
        std::vector<std::pair<std::string,std::string>> labels;
        if (DIR* d2 = ::opendir(base.c_str())) {
            while (auto* e2 = ::readdir(d2)) {
                if (!e2) continue;
                std::string fn = e2->d_name;
                if (fn.rfind("temp", 0)==0 && fn.size()>6 && ends_with(fn, "_label")) {
                    const std::string key = fn.substr(0, fn.size()-6);
                    const std::string lab = read_all(base + "/" + fn);
                    labels.emplace_back(key, lab);
                }
            }
            ::closedir(d2);
        }

        // collect inputs
        if (DIR* d2 = ::opendir(base.c_str())) {
            while (auto* e2 = ::readdir(d2)) {
                if (!e2) continue;
                std::string fn = e2->d_name;
                if (fn.rfind("temp", 0)==0 && fn.size()>6 && ends_with(fn, "_input")) {
                    const std::string key = fn.substr(0, fn.size()-6);
                    std::string lab = key;
                    auto it = std::find_if(labels.begin(), labels.end(),
                                           [&](const auto& p){ return p.first==key; });
                    if (it!=labels.end()) lab = it->second;

                    TempSensorInfo info;
                    // NOTE: TempSensorInfo has no 'device' field in current header.
                    // We only set fields that certainly exist: name/label/path.
                    info.name  = name;
                    info.label = lab;
                    info.path  = base + "/" + fn;
                    out.push_back(std::move(info));
                }
            }
            ::closedir(d2);
        }
    }

    ::closedir(d);
    return out;
}
