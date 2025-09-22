/*
 * Linux Fan Control — Vendor mapping (implementation)
 * (c) 2025 LinuxFanControl contributors
 */
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <optional>
#include <regex>
#include <shared_mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>
#include <nlohmann/json.hpp>
#include "include/Log.hpp"
#include "include/VendorMapping.hpp"

#if defined(__linux__)
#define LFC_HAS_INOTIFY 1
#include <sys/inotify.h>
#include <unistd.h>
#else
#define LFC_HAS_INOTIFY 0
#endif

namespace lfc {

using json = nlohmann::json;
namespace fs = std::filesystem;

struct VMRule {
    int priority{0};
    std::string vendor;
    std::string klass;
    std::regex re;
};

class VendorMappingImpl {
public:
    static VendorMappingImpl& inst() { static VendorMappingImpl I; return I; }

    void setSearchPaths(const std::vector<fs::path>& p) {
        std::unique_lock lk(mu_);
        searchPaths_ = p;
    }
    void setOverridePath(const fs::path& p) {
        std::unique_lock lk(mu_);
        overridePath_ = p;
    }
    void setWatchMode(VendorMapping::WatchMode m) {
        std::unique_lock lk(mu_);
        watchMode_ = m;
        restartWatchUnlocked_();
    }
    void setThrottleMs(int ms) {
        std::unique_lock lk(mu_);
        throttleMs_ = std::max(0, ms);
    }

    void ensureLoaded() {
        std::unique_lock lk(mu_);
        if (loaded_) return;
        lk.unlock();
        reloadNow_();
    }

    void tryReloadIfChanged() {
        std::unique_lock lk(mu_);
        if (activePath_.empty()) return;
        if (watchMode_ == VendorMapping::WatchMode::MTime) {
            const auto now = std::chrono::steady_clock::now();
            if (now - lastPoll_ < std::chrono::milliseconds(throttleMs_)) return;
            lastPoll_ = now;
            std::error_code ec;
            auto mt = fs::last_write_time(activePath_, ec);
            if (!ec && mt != activeMtime_) {
                lk.unlock();
                reloadNow_();
            }
        }
    }

    std::string vendorForChipName(const std::string& chip) const {
        std::shared_lock lk(mu_);
        for (const auto& r : rules_) {
            if (std::regex_search(chip, r.re)) return r.vendor.empty() ? chip : r.vendor;
        }
        return chip;
    }

    std::vector<std::string> chipAliasesFor(const std::string& token) const {
        std::shared_lock lk(mu_);
        auto it = chipAliases_.find(token);
        if (it == chipAliases_.end()) return {};
        return it->second;
    }

private:
    VendorMappingImpl() { restartWatchUnlocked_(); }

    static std::vector<fs::path> defaultSearchPaths_() {
        std::vector<fs::path> v;
        if (const char* p = std::getenv("LFC_VENDOR_MAP")) v.emplace_back(fs::path(p));
        if (const char* home = std::getenv("HOME"))
            v.emplace_back(fs::path(home) / ".config" / "LinuxFanControl" / "vendorMapping.json");
        v.emplace_back("/etc/LinuxFanControl/vendorMapping.json");
        v.emplace_back("/usr/share/LinuxFanControl/vendorMapping.json");
        return v;
    }

    static std::optional<fs::file_time_type> safeMTime_(const fs::path& p) {
        std::error_code ec;
        auto t = fs::last_write_time(p, ec);
        if (ec) return std::nullopt;
        return t;
    }

    static std::regex compileRegex_(const std::string& pattern) {
        std::string pat = pattern;
        bool icase = false;
        if (pat.size() >= 4 && pat.compare(0, 4, "(?i)") == 0) {
            icase = true;
            pat.erase(0, 4);
        }
        return std::regex(pat, std::regex::ECMAScript | (icase ? std::regex::icase : (std::regex_constants::syntax_option_type)0));
    }

    bool parseObject_(const json& j, std::string& err,
                      std::vector<VMRule>& outRules,
                      std::unordered_map<std::string,std::vector<std::string>>& outAliases)
    {
        if (!j.is_object()) { err = "vendorMapping.json must be an object"; return false; }
        if (!j.contains("rules") || !j["rules"].is_array()) { err = "missing 'rules' array"; return false; }

        for (const auto& r : j["rules"]) {
            if (!r.is_object()) continue;
            if (!r.contains("regex") || !r["regex"].is_string()) continue;
            const std::string pat = r["regex"].get<std::string>();
            const int pr  = (r.contains("priority") && r["priority"].is_number_integer()) ? r["priority"].get<int>() : 0;
            const std::string ven = (r.contains("vendor") && r["vendor"].is_string()) ? r["vendor"].get<std::string>() : "";
            const std::string cls = (r.contains("class")  && r["class"].is_string())  ? r["class"].get<std::string>()  : "";
            try {
                std::regex re = compileRegex_(pat);
                outRules.push_back(VMRule{pr, ven, cls, std::move(re)});
            } catch (...) {
                LOG_WARN("VendorMapping: bad regex ignored: %s", pat.c_str());
            }
        }
        std::sort(outRules.begin(), outRules.end(),
                  [](const VMRule& a, const VMRule& b){
                      if (a.priority != b.priority) return a.priority > b.priority;
                      return a.vendor < b.vendor;
                  });

        if (j.contains("chipAliases") && j["chipAliases"].is_object()) {
            for (auto it = j["chipAliases"].begin(); it != j["chipAliases"].end(); ++it) {
                if (!it.value().is_array()) continue;
                auto& vec = outAliases[it.key()];
                for (const auto& v : it.value()) if (v.is_string()) vec.push_back(v.get<std::string>());
            }
        }
        return true;
    }

    bool loadFromPath_(const fs::path& p, std::string& why) {
        std::ifstream f(p);
        if (!f) { why = "open failed"; return false; }
        json j = json::parse(f, nullptr, false);
        if (j.is_discarded()) { why = "invalid json"; return false; }

        std::vector<VMRule> newRules;
        std::unordered_map<std::string,std::vector<std::string>> newAliases;
        std::string err;
        if (!parseObject_(j, err, newRules, newAliases)) { why = err; return false; }

        auto mt = safeMTime_(p);
        {
            std::unique_lock lk(mu_);
            rules_.swap(newRules);
            chipAliases_.swap(newAliases);
            activePath_ = p;
            activeMtime_ = mt.value_or(fs::file_time_type{});
            loaded_ = true;
            lastPoll_ = std::chrono::steady_clock::now();
        }
        LOG_INFO("VendorMapping: loaded %s", p.string().c_str());
        return true;
    }

    void reloadNow_() {
        std::vector<fs::path> tryPaths;
        fs::path override;
        {
            std::shared_lock lk(mu_);
            tryPaths = searchPaths_.empty() ? defaultSearchPaths_() : searchPaths_;
            override = overridePath_;
        }

        if (!override.empty()) {
            std::string why;
            if (loadFromPath_(override, why)) return;
            LOG_WARN("VendorMapping: override failed: %s (%s)", why.c_str(), override.string().c_str());
        }

        for (const auto& p : tryPaths) {
            std::string why;
            if (loadFromPath_(p, why)) return;
        }
        LOG_WARN("VendorMapping: no usable vendorMapping.json found");
    }

    void restartWatchUnlocked_() {
#if LFC_HAS_INOTIFY
        if (watchThread_.joinable()) {
            stopWatch_.store(true);
            watchThread_.join();
            stopWatch_.store(false);
        }
        if (watchMode_ == VendorMapping::WatchMode::Inotify) {
            watchThread_ = std::thread([this]{
                int fd = inotify_init1(IN_NONBLOCK);
                if (fd < 0) return;
                int wd = -1;
                fs::path watched;
                while (!stopWatch_.load()) {
                    fs::path target;
                    {
                        std::shared_lock lk(mu_);
                        target = activePath_;
                    }
                    if (!target.empty()) {
                        fs::path dir = target.parent_path();
                        if (dir != watched) {
                            if (wd >= 0) { inotify_rm_watch(fd, wd); wd = -1; }
                            wd = inotify_add_watch(fd, dir.c_str(), IN_MODIFY | IN_CREATE | IN_MOVE | IN_DELETE);
                            watched = dir;
                        }
                    }
                    char buf[4096];
                    int n = read(fd, buf, sizeof(buf));
                    if (n > 0) {
                        tryReloadIfChanged();
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }
                if (wd >= 0) inotify_rm_watch(fd, wd);
                close(fd);
            });
        }
#else
        (void)stopWatch_;
        if (watchThread_.joinable()) { stopWatch_ = true; watchThread_.join(); stopWatch_ = false; }
#endif
    }

    mutable std::shared_mutex mu_;
    std::vector<fs::path> searchPaths_;
    fs::path overridePath_;

    std::vector<VMRule> rules_;
    std::unordered_map<std::string,std::vector<std::string>> chipAliases_;

    fs::path activePath_;
    fs::file_time_type activeMtime_{};
    std::chrono::steady_clock::time_point lastPoll_{};
    bool loaded_{false};

    VendorMapping::WatchMode watchMode_{VendorMapping::WatchMode::MTime};
    int throttleMs_{750};

    std::thread watchThread_;
    std::atomic<bool> stopWatch_{false};
};

static VendorMappingImpl& I() { return VendorMappingImpl::inst(); }

VendorMapping& VendorMapping::instance() {
    static VendorMapping V;
    I().ensureLoaded();
    return V;
}

void VendorMapping::setSearchPaths(const std::vector<fs::path>& p) { I().setSearchPaths(p); }
void VendorMapping::setOverridePath(const fs::path& p) { I().setOverridePath(p); }
void VendorMapping::setWatchMode(WatchMode m) { I().setWatchMode(m); }
void VendorMapping::setWatchMode(WatchMode m, int throttleMs) { I().setWatchMode(m); I().setThrottleMs(throttleMs); }
void VendorMapping::setThrottleMs(int ms) { I().setThrottleMs(ms); }
void VendorMapping::ensureLoaded() { I().ensureLoaded(); }
void VendorMapping::tryReloadIfChanged() { I().tryReloadIfChanged(); }
std::string VendorMapping::vendorForChipName(const std::string& chip) const { return I().vendorForChipName(chip); }
std::vector<std::string> VendorMapping::chipAliasesFor(const std::string& profileToken) const { return I().chipAliasesFor(profileToken); }

} // namespace lfc
