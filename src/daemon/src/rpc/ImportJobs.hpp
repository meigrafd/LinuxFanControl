#pragma once

#include <atomic>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>
#include <fstream>
#include <sstream>
#include <algorithm>

#include <nlohmann/json.hpp>

#include "include/Profile.hpp"
#include "include/Hwmon.hpp"
#include "include/FanControlImport.hpp"
#include "include/Log.hpp"
#include "include/Utils.hpp"

namespace lfc {

using nlohmann::json;

/* ------------------------------------------------------------------------- */
/* Status DTO used by RPC                                                    */
/* ------------------------------------------------------------------------- */
struct ImportStatus {
    std::string jobId;
    std::string state;              // "pending" | "running" | "done" | "error"
    int         progress{0};        // 0..100
    std::string message;            // short status text
    std::string error;              // set if state == "error"
    std::string profileName;        // target profile name (if known)
    bool        isFanControlRelease{false};
    json        details;            // importer diagnostics

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(ImportStatus,
        jobId, state, progress, message, error, profileName, isFanControlRelease, details)
};

/* Forward declaration so we can use it in ImportJob */
class ImportJobManager;

/* ------------------------------------------------------------------------- */
/* Import Job                                                                */
/* ------------------------------------------------------------------------- */
class ImportJob {
public:
    using ProgressFn = std::function<void(int /*pct*/, const std::string& /*msg*/)>;
    using CommitFn   = std::function<bool(const Profile&, std::string& err)>;

    ImportJob(std::string id,
              std::string srcPath,
              std::string dstName,
              bool validateDetect,
              int rpmMin,
              int timeoutMs)
        : id_(std::move(id)),
          path_(std::move(srcPath)),
          name_(std::move(dstName)),
          validateDetect_(validateDetect),
          rpmMin_(rpmMin),
          timeoutMs_(timeoutMs)
    {
        status_.jobId = id_;
        status_.state = "pending";
        status_.message = "queued";
        status_.profileName = name_;
        status_.details = json::object();
    }

    void start() {
        if (running_.exchange(true)) return;
        thr_ = std::thread([this]{ run_(); });
        thr_.detach();
    }

    bool cancel() {
        // Simple cooperative cancel: mark as error if still running.
        std::lock_guard<std::mutex> lk(mu_);
        if (status_.state == "running") {
            status_.state = "error";
            status_.error = "canceled";
            status_.message = "canceled";
            return true;
        }
        return false;
    }

    ImportStatus status() const {
        std::lock_guard<std::mutex> lk(mu_);
        return status_;
    }

    bool takeResult(Profile& out, std::string& err) {
        std::lock_guard<std::mutex> lk(mu_);
        if (status_.state != "done") {
            err = (status_.state == "error") ? status_.error : "not finished";
            return false;
        }
        if (!result_) { err = "no profile"; return false; }
        out = *result_;
        return true;
    }

    bool commit(const CommitFn& fn, std::string& err) {
        Profile p;
        if (!takeResult(p, err)) return false;
        if (!fn(p, err)) {
            std::lock_guard<std::mutex> lk(mu_);
            status_.state = "error";
            status_.error = err.empty() ? "commit failed" : err;
            status_.message = "commit failed";
            return false;
        }
        std::lock_guard<std::mutex> lk(mu_);
        status_.message = "committed";
        return true;
    }

private:
    /* ----------------------------- helpers ------------------------------ */

    void setState_(const std::string& st, int pct, const std::string& msg) {
        std::lock_guard<std::mutex> lk(mu_);
        status_.state = st;
        status_.progress = std::clamp(pct, 0, 100);
        status_.message = msg;
        if (st == "error" && status_.error.empty()) status_.error = msg;
        LOG_DEBUG("import: state=%s progress=%d msg=%s", st.c_str(), status_.progress, msg.c_str());
    }

    void fail_(const std::string& msg) {
        std::lock_guard<std::mutex> lk(mu_);
        status_.state = "error";
        status_.error = msg;
        status_.message = msg;
        LOG_ERROR("import: %s", msg.c_str());
    }

    void finish_(Profile&& p) {
        {
            std::lock_guard<std::mutex> lk(mu_);
            result_ = std::move(p);
            status_.state = "done";
            status_.progress = 100;
            status_.message = "done";
        }
        LOG_DEBUG("import: finished successfully");
    }

    struct LocalInv {
        std::vector<HwmonTemp> temps;
        std::vector<HwmonPwm>  pwms;
    };

    LocalInv getInventory_() const;

    std::string writeTempJson_(const json& j) const {
        // Keep temp path deterministic per job (matches existing logs)
        std::string tmp = "/tmp/lfc_import_" + id_ + ".json";
        std::ofstream os(tmp);
        os << j.dump(2);
        return tmp;
    }

    static bool looksLikeLfcProfile_(const json& j) {
        // A minimal heuristic (keep existing behavior): we consider LFC profile if it advertises schema containing "lfc.profile"
        try {
            return j.is_object()
                && j.contains("schema")
                && j.at("schema").is_string()
                && util::icontains(j.at("schema").get<std::string>(), "lfc.profile");
        } catch (...) {
            return false;
        }
    }

    static bool looksLikeFcr_(const json& j) {
        // FanControl ReFan format typically has top-level "FanCurves" and/or "Controls"
        return j.is_object() && (j.contains("FanCurves") || j.contains("Controls") || j.contains("Main"));
    }

    // Normalize FanControl/ReFan JSON into importer-friendly form, including sensor identifier parsing.
    json normalizeFcr_(const json& root) {
        json out = json::object();
        // keep original root to let FanControlImport handle details;
        // but provide a few normalized hints (like selected curve refs) if necessary.
        // We mostly pass-through here to avoid unnecessary behavior changes.
        out = root;
        return out;
    }

    /* ------------------------------- run -------------------------------- */

    void run_() {
        setState_("running", 0, "Reading source profile...");
        json j;
        try {
            // *** CHANGE: read JSON via utils helper (snake_case) ***
            j = util::read_json_file(path_);
        } catch (const std::exception& ex) {
            fail_(std::string("read failed: ") + ex.what());
            return;
        }

        setState_("running", 10, "Parsing...");
        try {
            if (j.is_object() && j.contains("Main") && j.at("Main").is_object()) {
                j = j.at("Main");
            }
        } catch (...) {
            // ignore, importer can still try best-effort
        }

        setState_("running", 20, "Detecting sensors...");
        const auto inv = getInventory_();

        const bool looksLfc = looksLikeLfcProfile_(j);
        const bool looksFcr = looksLikeFcr_(j);
        status_.isFanControlRelease = looksFcr;

        if (looksFcr) {
            isFcr_ = true;
            setState_("running", 40, "curves");
            const json jNorm = normalizeFcr_(j);

            const std::string tmp = writeTempJson_(jNorm);

            auto progress = [&](int pct, const std::string& msg){
                setState_("running", std::clamp(pct, 0, 99), msg);
            };

            Profile p;
            std::string err;
            nlohmann::json details; // filled by FanControlImport::LoadAndMap

            if (!FanControlImport::LoadAndMap(tmp, inv.temps, inv.pwms, p, err, progress, &details)) {
                {
                    std::lock_guard<std::mutex> lk(mu_);
                    status_.details = details;
                }
                fail_(err.empty() ? "FanControl import failed" : err);
                return;
            }

            if (!name_.empty()) p.name = name_;

            {
                std::lock_guard<std::mutex> lk(mu_);
                status_.details = details; // expose diagnostics
            }
            setState_("running", 98, "Finalizing...");
            finish_(std::move(p));
            return;
        }

        if (looksLfc) {
            // Keep behavior minimal: LFC profile import path not implemented here.
            fail_("unsupported profile format");
            return;
        }

        fail_("unsupported profile format");
    }

private:
    // Immutable inputs
    const std::string id_;
    const std::string path_;
    const std::string name_;
    const bool validateDetect_{false};
    const int  rpmMin_{0};
    const int  timeoutMs_{0};

    // State
    mutable std::mutex mu_;
    std::atomic<bool> running_{false};
    std::optional<Profile> result_;
    ImportStatus status_;
    bool isFcr_{false};
    std::thread thr_;
};

/* ------------------------------------------------------------------------- */
/* Manager                                                                   */
/* ------------------------------------------------------------------------- */
class ImportJobManager {
public:
    static ImportJobManager& instance() {
        static ImportJobManager s;
        return s;
    }

    // Create and start a new job
    std::string create(const std::string& path,
                       const std::string& name,
                       bool validateDetect,
                       int rpmMin,
                       int timeoutMs)
    {
        std::lock_guard<std::mutex> lk(mu_);
        const std::string id = std::to_string(++seq_);
        auto job = std::make_unique<ImportJob>(id, path, name, validateDetect, rpmMin, timeoutMs);
        auto* p = job.get();
        jobs_.emplace(id, std::move(job));
        p->start();
        return id;
    }

    bool cancel(const std::string& id, bool* found = nullptr) {
        std::lock_guard<std::mutex> lk(mu_);
        auto it = jobs_.find(id); if (found) *found = (it != jobs_.end());
        if (it == jobs_.end()) return false;
        return it->second->cancel();
    }

    using CommitFn = ImportJob::CommitFn;

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
        auto it = jobs_.find(id); if (found) *found = (it != jobs_.end());
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

    /* -------------------- primed inventory cache (daemon) ---------------- */

    // Daemon provides its hwmon snapshot once at startup.
    void primeInventory(const std::vector<HwmonTemp>& temps,
                        const std::vector<HwmonPwm>&  pwms)
    {
        std::lock_guard<std::mutex> lk(invMu_);
        cachedTemps_ = temps;
        cachedPwms_  = pwms;
    }

    // ImportJob reuses daemon inventory instead of rescanning.
    bool getCachedInventory(std::vector<HwmonTemp>& temps,
                            std::vector<HwmonPwm>&  pwms) const
    {
        std::lock_guard<std::mutex> lk(invMu_);
        if (cachedTemps_.empty() && cachedPwms_.empty()) return false;
        temps = cachedTemps_;
        pwms  = cachedPwms_;
        return true;
    }

private:
    ImportJobManager() = default;

    mutable std::mutex mu_;
    std::map<std::string, std::unique_ptr<ImportJob>> jobs_;
    uint64_t seq_{0};

    // cached daemon inventory (optional)
    mutable std::mutex invMu_;
    std::vector<HwmonTemp> cachedTemps_;
    std::vector<HwmonPwm>  cachedPwms_;
};

/* ------------------------------------------------------------------------- */
/* ImportJob methods depending on ImportJobManager                            */
/* ------------------------------------------------------------------------- */

inline ImportJob::LocalInv ImportJob::getInventory_() const {
    LocalInv inv;
    // Prefer cached inventory provided by the daemon (primed via ImportJobManager).
    if (ImportJobManager::instance().getCachedInventory(inv.temps, inv.pwms)) {
        LOG_DEBUG("import: using cached hwmon snapshot (temps=%zu pwms=%zu)",
                  inv.temps.size(), inv.pwms.size());
        return inv;
    }
    // Fallback: if daemon didn't prime, do a single scan as safety net.
    const auto s = Hwmon::scan();
    inv.temps = s.temps;
    inv.pwms  = s.pwms;
    return inv;
}

} // namespace lfc
