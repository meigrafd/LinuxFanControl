/*
 * Linux Fan Control — Import job manager (RPC types & orchestration)
 * (c) 2025 LinuxFanControl contributors
 */
#pragma once
#include <algorithm>
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
#include <nlohmann/json.hpp>
#include "include/Version.hpp"
#include "include/Profile.hpp"
#include "include/Hwmon.hpp"
#include "include/Log.hpp"
#include "include/FanControlImport.hpp"

namespace lfc {

struct ImportStatus {
    std::string    jobId;
    std::string    state;
    int            progress{0};
    std::string    message;
    std::string    error;
    std::string    profileName;
    bool           isFanControlRelease{false};
    nlohmann::json details; // mapping/diagnostics for UI

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(ImportStatus,
        jobId,state,progress,message,error,profileName,isFanControlRelease,details)
};

class ImportJob {
public:
    ImportJob(std::string id, std::string path, std::string name,
              bool validateDetect, int rpmMin, int timeoutMs)
        : id_(std::move(id)), path_(std::move(path)), name_(std::move(name)),
          validateDetect_(validateDetect), rpmMin_(rpmMin), timeoutMs_(timeoutMs) {}

    void start() {
        if (running_.exchange(true)) return;
        thr_ = std::thread([this]{ run(); });
        thr_.detach();
    }

    ImportStatus status() const {
        std::lock_guard<std::mutex> lk(mu_);
        ImportStatus s;
        s.jobId      = id_;
        s.state      = state_;
        s.progress   = progress_;
        s.message    = message_;
        s.error      = error_;
        s.profileName= result_ ? result_->name : "";
        s.isFanControlRelease = isFcr_;
        s.details    = details_;
        return s;
    }

    bool takeResult(Profile& out, std::string& err) {
        std::lock_guard<std::mutex> lk(mu_);
        if (state_ != "done") { err = (state_=="error") ? error_ : "not finished"; return false; }
        out = *result_;
        return true;
    }

    bool cancel() {
        std::lock_guard<std::mutex> lk(mu_);
        if (state_=="pending" || state_=="running") { state_="error"; error_="canceled"; progress_=0; return true; }
        return false;
    }

private:
    static bool readFileToString_(const std::string& path, std::string& out) {
        std::ifstream f(path);
        if (!f) return false;
        std::ostringstream ss; ss << f.rdbuf(); out = ss.str(); return true;
    }

    void setState_(const std::string& st, int pct, const std::string& msg) {
        std::lock_guard<std::mutex> lk(mu_);
        state_   = st;
        progress_= pct;
        message_ = msg;
    }
    void fail_(const std::string& e) {
        std::lock_guard<std::mutex> lk(mu_);
        state_ = "error"; error_ = e; progress_ = 0;
    }
    void finish_(Profile&& p) {
        std::lock_guard<std::mutex> lk(mu_);
        state_ = "done"; progress_ = 100; message_ = "OK"; result_ = std::move(p);
    }

    static nlohmann::json normalizeFcr_(const nlohmann::json& src) {
        using json = nlohmann::json; json out = src;
        const bool hasMain = src.contains("Main") && src["Main"].is_object();
        if (!out.contains("Controls") && hasMain && src["Main"].contains("Controls")) out["Controls"] = src["Main"]["Controls"];
        if (!out.contains("Curves") && hasMain && src["Main"].contains("FanCurves")) out["Curves"] = src["Main"]["FanCurves"];
        if (!out.contains("TemperatureSensors") && hasMain && src["Main"].contains("TemperatureSensors")) out["TemperatureSensors"] = src["Main"]["TemperatureSensors"];
        return out;
    }

    static std::string writeTempJson_(const nlohmann::json& j, const std::string& jobId){
        std::string path = "/tmp/lfc_import_" + jobId + ".json";
        std::ofstream f(path, std::ios::binary);
        if (!f) return {};
        const auto dumped = j.dump(2);
        f.write(dumped.data(), (std::streamsize)dumped.size());
        return f ? path : std::string{};
    }

    void run() {
        setState_("running", 0, "Reading source profile...");
        std::string blob; if (!readFileToString_(path_, blob)) { fail_("cannot open file"); return; }
        nlohmann::json j = nlohmann::json::parse(blob, nullptr, false);
        if (j.is_discarded()) { fail_("invalid JSON"); return; }
        setState_("running", 10, "Parsing...");

        const bool isLfc =
            j.contains("schema") && j["schema"].is_string() &&
            j["schema"].get<std::string>().rfind("LinuxFanControl.Profile", 0) == 0;
        const bool looksFcr =
            j.contains("Computers") || j.contains("Controls") || j.contains("Curves") ||
            (j.contains("Version") && j["Version"].is_string()) || j.contains("__VERSION__") ||
            (j.contains("Main") && j["Main"].is_object() &&
             (j["Main"].contains("Controls") || j["Main"].contains("TemperatureSensors") || j["Main"].contains("Curves")));

        Profile p{}; p.name = !name_.empty()? name_ : "Imported"; p.schema = "LinuxFanControl.Profile/v1"; p.lfcdVersion = LFCD_VERSION;

        if (isLfc) {
            try {
                from_json(j, p);
                if (!name_.empty()) p.name = name_;
                p.lfcdVersion = LFCD_VERSION;
                setState_("running", 70, "Validated LinuxFanControl profile");
            } catch (...) {
                fail_("LFC import failed");
                return;
            }
        } else if (looksFcr) {
            isFcr_ = true;
            setState_("running", 20, "Detecting sensors...");
            const auto inv  = Hwmon::scan();
            const auto jNorm= normalizeFcr_(j);
            const std::string tmp = writeTempJson_(jNorm, id_);
            if (tmp.empty()) { fail_("cannot create temp file for normalized FanControl profile"); return; }

            auto progress = [this](int pct, const std::string& msg) { setState_("running", std::clamp(pct, 20, 95), msg); };

            std::string err;
            nlohmann::json details; // will be filled by importer
            if (!FanControlImport::LoadAndMap(tmp, inv.temps, inv.pwms, p, err, progress, &details)) {
                details_ = std::move(details);
                fail_(err.empty()? "FanControl import failed" : err);
                return;
            }
            if (!name_.empty()) p.name = name_;
            details_ = std::move(details);
            setState_("running", 98, "Finalizing...");
        } else {
            fail_("unsupported profile format");
            return;
        }

        finish_(std::move(p));
    }

    std::string id_, path_, name_;
    bool  validateDetect_{false};
    int   rpmMin_{0}, timeoutMs_{0};

    mutable std::mutex mu_;
    std::string state_{"pending"}, message_, error_;
    int progress_{0};
    std::optional<Profile> result_;
    bool isFcr_{false};
    nlohmann::json details_;

    std::atomic<bool> running_{false};
    std::thread thr_;
};

class ImportJobManager {
public:
    static ImportJobManager& instance(){ static ImportJobManager s; return s; }

    std::string create(const std::string& path,const std::string& name,bool vdet,int rpmMin,int timeoutMs){
        std::lock_guard<std::mutex> lk(mu_);
        const std::string id = std::to_string(++seq_);
        auto job = std::make_unique<ImportJob>(id, path, name, vdet, rpmMin, timeoutMs);
        auto* p = job.get();
        jobs_.emplace(id, std::move(job));
        p->start();
        return id;
    }

    bool cancel(const std::string& id,bool*found=nullptr){
        std::lock_guard<std::mutex> lk(mu_);
        auto it = jobs_.find(id); if (found) *found = (it != jobs_.end());
        if (it == jobs_.end()) return false;
        return it->second->cancel();
    }

    using CommitFn=std::function<bool(const Profile&,std::string&err)>;
    bool commit(const std::string& id, CommitFn fn, std::string& err){
        std::unique_ptr<ImportJob> job;
        { std::lock_guard<std::mutex> lk(mu_);
          auto it = jobs_.find(id);
          if (it == jobs_.end()) { err="job not found"; return false; }
          job = std::move(it->second); jobs_.erase(it);
        }
        Profile p; if (!job->takeResult(p, err)) return false; return fn(p, err);
    }

    ImportStatus get(const std::string& id,bool*found=nullptr)const{
        std::lock_guard<std::mutex> lk(mu_);
        auto it = jobs_.find(id); if (found) *found = (it != jobs_.end());
        if (it == jobs_.end()) return {};
        return it->second->status();
    }

    std::vector<ImportStatus> list()const{
        std::vector<ImportStatus> r;
        std::lock_guard<std::mutex> lk(mu_);
        r.reserve(jobs_.size());
        for (const auto& kv : jobs_) r.push_back(kv.second->status());
        return r;
    }

private:
    mutable std::mutex mu_;
    std::map<std::string,std::unique_ptr<ImportJob>> jobs_;
    uint64_t seq_{0};
};

} // namespace lfc
