/*
 * Linux Fan Control — Import job manager (header-only)
 * Async import pipeline for LinuxFanControl and FanControl.Release profiles
 * (c) 2025 LinuxFanControl contributors
 */
#pragma once

#include <nlohmann/json.hpp>
#include <atomic>
#include <chrono>
#include <functional>
#include <fstream>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "include/Version.hpp"
#include "include/Profile.hpp"
#include "include/Hwmon.hpp"
#include "include/Log.hpp"

namespace lfc {

// Forward declaration for importer API; implemented elsewhere.
class FanControlImport {
public:
    using ProgressFn = std::function<void(int, const std::string&)>;
    static bool LoadAndMap(const std::string& path,
                           const std::vector<HwmonTemp>& temps,
                           const std::vector<HwmonPwm>& pwms,
                           Profile& out,
                           std::string& err,
                           ProgressFn onProgress);
};

struct ImportStatus {
    std::string jobId;
    std::string state;     // "pending" | "running" | "done" | "error"
    int         progress{0};
    std::string message;
    std::string error;
    std::string profileName;
    bool        isFanControlRelease{false};
};

class ImportJob {
public:
    using ProgressFn = std::function<void(int, const std::string&)>;

    ImportJob(std::string id, std::string path, std::string asName,
              bool validateDet, int rpmMin, int timeoutMs)
        : id_(std::move(id)),
          path_(std::move(path)),
          asName_(std::move(asName)),
          validateDet_(validateDet),
          rpmMin_(rpmMin),
          timeoutMs_(timeoutMs) {}

    void start() {
        if (running_.exchange(true)) return;
        thr_ = std::thread([this]{ run(); });
        thr_.detach();
    }

    ImportStatus status() const {
        std::lock_guard<std::mutex> lk(mu_);
        ImportStatus s;
        s.jobId   = id_;
        s.state   = state_;
        s.progress= progress_;
        s.message = message_;
        s.error   = error_;
        s.profileName = result_.has_value() ? result_->name : std::string{};
        s.isFanControlRelease = isFcr_;
        return s;
    }

    bool takeResult(Profile& out, std::string& err) {
        std::lock_guard<std::mutex> lk(mu_);
        if (state_ != "done") {
            err = (state_ == "error") ? error_ : "not finished";
            return false;
        }
        out = *result_;
        return true;
    }

    bool cancel() {
        std::lock_guard<std::mutex> lk(mu_);
        if (state_ == "pending" || state_ == "running") {
            state_ = "error";
            error_ = "canceled";
            return true;
        }
        return false;
    }

private:
    // Manager has privileged access.
    friend class ImportJobManager;

    static bool readFileToString_(const std::string& path, std::string& out) {
        std::ifstream f(path);
        if (!f) return false;
        std::ostringstream ss; ss << f.rdbuf();
        out = ss.str();
        return true;
    }

    void setState_(const std::string& st, int pct, const std::string& msg) {
        std::lock_guard<std::mutex> lk(mu_);
        state_   = st;
        progress_= pct;
        message_ = msg;
    }
    void fail_(const std::string& e) {
        std::lock_guard<std::mutex> lk(mu_);
        state_ = "error";
        error_ = e;
        progress_ = 0;
    }
    void finish_(Profile&& p) {
        std::lock_guard<std::mutex> lk(mu_);
        state_   = "done";
        progress_= 100;
        message_ = "OK";
        result_  = std::move(p);
    }

    void run() {
        setState_("running", 0, "Reading source profile...");

        std::string blob;
        if (!readFileToString_(path_, blob)) { fail_("cannot open file"); return; }

        nlohmann::json j = nlohmann::json::parse(blob, nullptr, false);
        if (j.is_discarded()) { fail_("invalid JSON"); return; }

        const bool isLfc = j.contains("schema") && j["schema"].is_string()
                           && j["schema"].get<std::string>().rfind("LinuxFanControl.Profile", 0) == 0;
        const bool looksFcr = j.contains("Computers") || j.contains("Controls") || j.contains("Curves")
                              || (j.contains("Version") && j["Version"].is_string());

        Profile p{};
        p.name = !asName_.empty() ? asName_ : "Imported";
        p.schema = "LinuxFanControl.Profile/v1";
        p.lfcdVersion = LFCD_VERSION;

        if (isLfc) {
            try {
                from_json(j, p);
                if (!asName_.empty()) p.name = asName_;
                p.lfcdVersion = LFCD_VERSION;
                setState_("running", 70, "Validated LinuxFanControl profile");
            } catch (const std::exception& ex) {
                fail_(std::string("LFC import failed: ") + ex.what());
                return;
            }
        } else if (looksFcr) {
            isFcr_ = true;
            setState_("running", 10, "Detecting sensors...");

            auto inv = Hwmon::scan();
            auto progress = [this](int pct, const std::string& msg){
                setState_("running", pct, msg);
            };

            std::string err;
            if (!FanControlImport::LoadAndMap(path_, inv.temps, inv.pwms, p, err, progress)) {
                fail_(err.empty() ? "mapping failed" : err);
                return;
            }

            setState_("running", 85, "Mapped controls and curves");
        } else {
            fail_("unrecognized profile format");
            return;
        }

        if (validateDet_) {
            setState_("running", 90, "Validating fan response...");
            std::string vErr;
            if (!validateControls_(p, vErr)) {
                fail_(vErr.empty()? "validation failed" : vErr);
                return;
            }

            if (rpmMin_ > 0) {
                setState_("running", 95, "Checking tach threshold...");
                std::string rErr;
                if (!verifyRpmThreshold_(p, rpmMin_, timeoutMs_, rErr)) {
                    fail_(rErr.empty()? "rpm check failed" : rErr);
                    return;
                }
            }
        }

        finish_(std::move(p));
    }

    bool validateControls_(const Profile& prof, std::string& err) const {
        const auto inv = Hwmon::scan();

        std::multimap<std::string, const HwmonFan*> fansByChip;
        for (const auto& f : inv.fans) fansByChip.emplace(f.chipPath, &f);

        auto findPwmByPath = [&](const std::string& path) -> const HwmonPwm* {
            for (const auto& w : inv.pwms) if (w.path_pwm == path) return &w;
            return nullptr;
        };

        for (const auto& ctl : prof.controls) {
            const HwmonPwm* pwm = findPwmByPath(ctl.pwmPath);
            if (!pwm) { err = "unknown pwmPath: " + ctl.pwmPath; return false; }

            auto range = fansByChip.equal_range(pwm->chipPath);
            if (range.first == range.second) { err = "no fan tach on chip: " + pwm->chipPath; return false; }
        }
        return true;
    }

    bool verifyRpmThreshold_(const Profile& p, int rpmMin, int timeoutMs, std::string& err) const {
        const auto inv = Hwmon::scan();

        std::multimap<std::string, const HwmonFan*> fansByChip;
        for (const auto& f : inv.fans) fansByChip.emplace(f.chipPath, &f);

        auto findPwmByPath = [&](const std::string& path) -> const HwmonPwm* {
            for (const auto& w : inv.pwms) if (w.path_pwm == path) return &w;
            return nullptr;
        };

        const int pollMs = 100;
        for (const auto& ctl : p.controls) {
            const HwmonPwm* pwm = findPwmByPath(ctl.pwmPath);
            if (!pwm) { err = "pwm not found: " + ctl.pwmPath; return false; }

            int prevEnable = Hwmon::readEnable(*pwm).value_or(2);
            int prevRaw    = Hwmon::readRaw(*pwm).value_or(0);

            auto range = fansByChip.equal_range(pwm->chipPath);
            std::vector<const HwmonFan*> fans;
            for (auto it = range.first; it != range.second; ++it) fans.push_back(it->second);
            if (fans.empty()) { err = "no fan tach on chip: " + pwm->chipPath; return false; }

            (void)Hwmon::setPercent(*pwm, 100);
            const auto t0 = std::chrono::steady_clock::now();
            bool ok = false;
            while (true) {
                for (auto* f : fans) {
                    int rpm = Hwmon::readRpm(*f).value_or(-1);
                    if (rpm >= rpmMin) { ok = true; break; }
                }
                if (ok) break;
                auto dt = std::chrono::duration_cast<std::chrono::milliseconds>(
                              std::chrono::steady_clock::now() - t0).count();
                if (timeoutMs > 0 && dt >= timeoutMs) break;
                std::this_thread::sleep_for(std::chrono::milliseconds(pollMs));
            }

            Hwmon::writeEnable(pwm->path_enable, prevEnable);
            if (prevEnable == 1 || prevEnable == 2) Hwmon::writeRaw(pwm->path_pwm, prevRaw);

            if (!ok) { err = "rpm threshold not reached on chip " + pwm->chipPath; return false; }
        }
        return true;
    }

private:
    std::string id_;
    std::string path_;
    std::string asName_;

    bool validateDet_{false};
    int  rpmMin_{0};
    int  timeoutMs_{0};

    mutable std::mutex mu_;
    std::string state_{"pending"};
    int         progress_{0};
    std::string message_;
    std::string error_;
    std::optional<Profile> result_;
    std::atomic<bool> running_{false};
    std::thread thr_;
    bool isFcr_{false};
};

// -----------------------------------------------------------------------------

class ImportJobManager {
public:
    static ImportJobManager& instance() {
        static ImportJobManager inst;
        return inst;
    }

    std::string create(const std::string& path, const std::string& asName,
                       bool validateDet, int rpmMin, int timeoutMs) {
        std::lock_guard<std::mutex> lk(mu_);
        const std::string id = std::to_string(++seq_);
        auto job = std::make_unique<ImportJob>(id, path, asName, validateDet, rpmMin, timeoutMs);
        auto* ptr = job.get();
        jobs_.emplace(id, std::move(job));
        ptr->start();
        return id;
    }

    bool cancel(const std::string& id, bool* found = nullptr) {
        std::lock_guard<std::mutex> lk(mu_);
        auto it = jobs_.find(id);
        if (found) *found = (it != jobs_.end());
        if (it == jobs_.end()) return false;
        return it->second->cancel();
    }

    using CommitFn = std::function<bool(const Profile&, std::string& err)>;
    bool commit(const std::string& id, CommitFn fn, std::string& err) {
        std::unique_ptr<ImportJob> job;
        {
            std::lock_guard<std::mutex> lk(mu_);
            auto it = jobs_.find(id);
            if (it == jobs_.end()) { err = "job not found"; return false; }
            job = std::move(it->second);
            jobs_.erase(it);
        }
        Profile p;
        if (!job->takeResult(p, err)) return false;
        return fn(p, err);
    }

    ImportStatus get(const std::string& id, bool* found = nullptr) const {
        std::lock_guard<std::mutex> lk(mu_);
        auto it = jobs_.find(id);
        if (found) *found = (it != jobs_.end());
        if (it == jobs_.end()) return {};
        return it->second->status();
    }

    std::vector<ImportStatus> list() const {
        std::vector<ImportStatus> r;
        std::lock_guard<std::mutex> lk(mu_);
        r.reserve(jobs_.size());
        for (const auto& kv : jobs_) r.push_back(kv.second->status());
        return r;
    }

    void snapshot(std::vector<ImportJob>& out) const { out.clear(); }

    bool runSync(const std::string& id) {
        std::unique_ptr<ImportJob>* slot = nullptr;
        {
            std::lock_guard<std::mutex> lk(mu_);
            auto it = jobs_.find(id);
            if (it == jobs_.end()) return false;
            slot = const_cast<std::unique_ptr<ImportJob>*>(&it->second);
        }
        if (!slot || !(*slot)) return false;
        (*slot)->run();
        return true;
    }

    std::map<std::string, std::unique_ptr<ImportJob>>* jobsPtr() { return &jobs_; }

private:
    mutable std::mutex mu_;
    std::map<std::string, std::unique_ptr<ImportJob>> jobs_;
    uint64_t seq_{0};
};

} // namespace lfc
