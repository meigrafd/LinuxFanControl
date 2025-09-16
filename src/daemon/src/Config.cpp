/*
 * Linux Fan Control — Daemon Config (implementation)
 * - Robust JSON parsing with strict type checks
 * - Env override via LFCD_*
 * - Atomic save with 0600 permissions
 * (c) 2025 LinuxFanControl contributors
 */
#include "Config.hpp"

#include <cstdlib>
#include <cctype>
#include <fstream>
#include <sstream>
#include <system_error>
#include <filesystem>
#include <nlohmann/json.hpp>

namespace fs = std::filesystem;
using nlohmann::json;

namespace lfc {

// ---------- env helpers ----------

static const char* getenv_c(const char* k) {
    const char* v = std::getenv(k);
    return v && *v ? v : nullptr;
}

static std::string getenv_or(const char* k, const std::string& def) {
    if (auto* v = getenv_c(k)) return std::string(v);
    return def;
}

static int getenv_or_int(const char* k, int def) {
    if (auto* v = getenv_c(k)) {
        try { return std::stoi(v); } catch (...) {}
    }
    return def;
}

static double getenv_or_double(const char* k, double def) {
    if (auto* v = getenv_c(k)) {
        try { return std::stod(v); } catch (...) {}
    }
    return def;
}

bool parseBool(const std::string& s) {
    std::string t;
    t.reserve(s.size());
    for (char c : s) t.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    return (t == "1" || t == "true" || t == "yes" || t == "on");
}

std::string expandUserPath(const std::string& p) {
    if (p.size() >= 2 && p[0] == '~' && p[1] == '/') {
        const char* home = getenv_c("HOME");
        if (home && *home) {
            std::string out = home;
            if (!out.empty() && out.back() != '/') out.push_back('/');
            out.append(p.begin() + 2, p.end());
            return out;
        }
    }
    return p;
}

static std::string readFile(const fs::path& path) {
    std::ifstream f(path);
    std::ostringstream oss;
    oss << f.rdbuf();
    return oss.str();
}

// ---------- robust json -> cfg ----------

static void set_if_string(const json& j, const char* key, std::string& dst) {
    auto it = j.find(key);
    if (it != j.end() && it->is_string()) dst = it->get<std::string>();
}

static void set_if_bool(const json& j, const char* key, bool& dst) {
    auto it = j.find(key);
    if (it != j.end()) {
        if (it->is_boolean()) dst = it->get<bool>();
        else if (it->is_string()) dst = parseBool(it->get<std::string>());
        else if (it->is_number_integer()) dst = (it->get<int>() != 0);
    }
}

static void set_if_int(const json& j, const char* key, int& dst) {
    auto it = j.find(key);
    if (it != j.end()) {
        if (it->is_number_integer()) dst = it->get<int>();
        else if (it->is_string()) {
            try { dst = std::stoi(it->get<std::string>()); } catch (...) {}
        }
    }
}

static void set_if_double(const json& j, const char* key, double& dst) {
    auto it = j.find(key);
    if (it != j.end()) {
        if (it->is_number_float()) dst = it->get<double>();
        else if (it->is_number_integer()) dst = static_cast<double>(it->get<int>());
        else if (it->is_string()) {
            try { dst = std::stod(it->get<std::string>()); } catch (...) {}
        }
    }
}

static int clampi(int v, int lo, int hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static double clampd(double v, double lo, double hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static void json_to_cfg(const json& j, DaemonConfig& c) {
    // Logging/runtime
    set_if_string(j, "logLevel", c.logLevel);
    set_if_bool(j,   "debug", c.debug);
    set_if_bool(j,   "foreground", c.foreground);
    set_if_bool(j,   "cmds", c.cmds);

    // Networking
    set_if_string(j, "host", c.host);
    set_if_int(j,    "port", c.port);

    // Paths
    set_if_string(j, "pidfile", c.pidfile);
    set_if_string(j, "logfile", c.logfile);
    set_if_string(j, "shm", c.shmPath);

    // Sensors
    set_if_string(j, "sysfsRoot", c.sysfsRoot);
    set_if_string(j, "sensorsBackend", c.sensorsBackend);

    // Profiles (locations only)
    set_if_string(j, "profilesDir", c.profilesDir);
    set_if_string(j, "profile", c.profileName);

    // Loop tuning
    set_if_int(j,    "tickMs", c.tickMs);
    set_if_double(j, "deltaC", c.deltaC);
    set_if_int(j,    "forceTickMs", c.forceTickMs);

    // clamp ranges
    c.tickMs      = clampi(c.tickMs, 5, 1000);
    c.deltaC      = clampd(c.deltaC, 0.0, 10.0);
    c.forceTickMs = clampi(c.forceTickMs, 100, 10000);
}

static json cfg_to_json(const DaemonConfig& c) {
    json j;
    j["logLevel"]       = c.logLevel;
    j["debug"]          = c.debug;
    j["foreground"]     = c.foreground;
    j["cmds"]           = c.cmds;

    j["host"]           = c.host;
    j["port"]           = c.port;

    j["pidfile"]        = c.pidfile;
    j["logfile"]        = c.logfile;
    j["shm"]            = c.shmPath;

    j["sysfsRoot"]      = c.sysfsRoot;
    j["sensorsBackend"] = c.sensorsBackend;

    j["profilesDir"]    = c.profilesDir;
    j["profile"]        = c.profileName;

    j["tickMs"]         = c.tickMs;
    j["deltaC"]         = c.deltaC;
    j["forceTickMs"]    = c.forceTickMs;

    return j;
}

static void apply_env_overrides(DaemonConfig& c) {
    if (auto* v = getenv_c("LFCD_LOG_LEVEL"))    c.logLevel   = v;
    if (auto* v = getenv_c("LFCD_DEBUG"))        c.debug      = parseBool(v);
    if (auto* v = getenv_c("LFCD_FOREGROUND"))   c.foreground = parseBool(v);
    if (auto* v = getenv_c("LFCD_CMDS"))         c.cmds       = parseBool(v);

    if (auto* v = getenv_c("LFCD_HOST"))         c.host       = v;
    c.port = getenv_or_int("LFCD_PORT", c.port);

    if (auto* v = getenv_c("LFCD_PIDFILE"))      c.pidfile    = v;
    if (auto* v = getenv_c("LFCD_LOGFILE"))      c.logfile    = v;
    if (auto* v = getenv_c("LFCD_SHM"))          c.shmPath    = v;

    if (auto* v = getenv_c("LFCD_SYSFS_ROOT"))       c.sysfsRoot      = v;
    if (auto* v = getenv_c("LFCD_SENSORS_BACKEND"))  c.sensorsBackend = v;

    if (auto* v = getenv_c("LFCD_PROFILES_DIR")) c.profilesDir = v;
    if (auto* v = getenv_c("LFCD_PROFILE"))      c.profileName = v;

    // Loop tuning (with range clamp)
    int tms = getenv_or_int("LFCD_TICK_MS", c.tickMs);
    int ftm = getenv_or_int("LFCD_FORCE_TICK_MS", c.forceTickMs);
    double dc = getenv_or_double("LFCD_DELTA_C", c.deltaC);
    c.tickMs      = (tms ? tms : c.tickMs);
    c.forceTickMs = (ftm ? ftm : c.forceTickMs);
    c.deltaC      = dc;

    c.tickMs      = clampi(c.tickMs, 5, 1000);
    c.deltaC      = clampd(c.deltaC, 0.0, 10.0);
    c.forceTickMs = clampi(c.forceTickMs, 100, 10000);
}

// ---------- public API ----------

DaemonConfig loadDaemonConfig(const std::string& configPath, std::string* err) {
    DaemonConfig cfg;

    // 1) Load JSON file (robust to type mismatches)
    if (!configPath.empty()) {
        try {
            fs::path p = expandUserPath(configPath);
            if (fs::exists(p)) {
                auto s = readFile(p);
                auto j = json::parse(s, /*cb*/nullptr, /*allow_exceptions*/true);
                json_to_cfg(j, cfg);
                cfg.configFile = p.string();
            } else if (err) {
                *err = "config file not found: " + p.string();
            }
        } catch (const std::exception& ex) {
            if (err) *err = std::string("failed to load config: ") + ex.what();
        }
    }

    // 2) Env overrides (LFCD_*)
    apply_env_overrides(cfg);

    // 3) Expand and sanitize
    cfg.pidfile     = expandUserPath(cfg.pidfile);
    cfg.logfile     = expandUserPath(cfg.logfile);
    cfg.shmPath     = expandUserPath(cfg.shmPath);
    cfg.profilesDir = expandUserPath(cfg.profilesDir);

    if (cfg.port <= 0 || cfg.port > 65535) cfg.port = 8777;
    if (cfg.logLevel.empty()) cfg.logLevel = "info";
    if (cfg.sensorsBackend != "auto" && cfg.sensorsBackend != "sysfs" && cfg.sensorsBackend != "libsensors") {
        cfg.sensorsBackend = "auto";
    }

    return cfg;
}

DaemonConfig loadDaemonConfig(std::string* err) {
    std::string path = getenv_or("LFCD_CONFIG", "");
    if (path.empty()) {
        const char* home = getenv_c("HOME");
        if (home && *home) {
            path = std::string(home) + "/.config/LinuxFanControl/daemon.json";
        }
    }
    return loadDaemonConfig(path, err);
}

static bool atomic_write(const fs::path& target, const std::string& data, std::string* err) {
    try {
        fs::path dir = target.parent_path();
        if (!dir.empty()) {
            fs::create_directories(dir);
        }
        fs::path tmp = target;
        tmp += ".tmp";

        {
            std::ofstream of(tmp, std::ios::binary | std::ios::trunc);
            if (!of) {
                if (err) *err = "open temp for write failed: " + tmp.string();
                return false;
            }
            of.write(data.data(), static_cast<std::streamsize>(data.size()));
            if (!of) {
                if (err) *err = "write failed: " + tmp.string();
                return false;
            }
            of.flush();
            if (!of) {
                if (err) *err = "flush failed: " + tmp.string();
                return false;
            }
        }

        std::error_code ec;
        fs::permissions(tmp, fs::perms::owner_read | fs::perms::owner_write,
                        fs::perm_options::replace, ec);

        fs::rename(tmp, target, ec);
        if (ec) {
            if (err) *err = "rename failed: " + ec.message();
            return false;
        }
        return true;
    } catch (const std::exception& ex) {
        if (err) *err = ex.what();
        return false;
    }
}

bool saveDaemonConfig(const DaemonConfig& cfg, const std::string& path, std::string* err) {
    fs::path p = expandUserPath(path);
    json j = cfg_to_json(cfg);
    std::string payload = j.dump(2);
    return atomic_write(p, payload, err);
}

bool saveDaemonConfig(const DaemonConfig& cfg, std::string* err) {
    std::string path = cfg.configFile;
    if (path.empty()) {
        path = getenv_or("LFCD_CONFIG", "");
        if (path.empty()) {
            const char* home = getenv_c("HOME");
            if (home && *home) {
                path = std::string(home) + "/.config/LinuxFanControl/daemon.json";
            } else {
                if (err) *err = "cannot resolve HOME for default config path";
                return false;
            }
        }
    }
    return saveDaemonConfig(cfg, path, err);
}

} // namespace lfc
