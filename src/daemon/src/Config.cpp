/*
 * Linux Fan Control — Daemon Config (implementation)
 * - Daemon/runtime configuration (no profiles here)
 * - Env override via LFCD_* variables
 * - JSON save with atomic write
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

static void json_to_cfg(const json& j, DaemonConfig& c) {
    if (j.contains("logLevel"))     c.logLevel   = j.value("logLevel", c.logLevel);
    if (j.contains("debug"))        c.debug      = j.value("debug", c.debug);
    if (j.contains("foreground"))   c.foreground = j.value("foreground", c.foreground);
    if (j.contains("cmds"))         c.cmds       = j.value("cmds", c.cmds);

    if (j.contains("host"))         c.host       = j.value("host", c.host);
    if (j.contains("port"))         c.port       = j.value("port", c.port);

    if (j.contains("pidfile"))      c.pidfile    = j.value("pidfile", c.pidfile);
    if (j.contains("logfile"))      c.logfile    = j.value("logfile", c.logfile);
    if (j.contains("shm"))          c.shmPath    = j.value("shm", c.shmPath);

    if (j.contains("sysfsRoot"))    c.sysfsRoot      = j.value("sysfsRoot", c.sysfsRoot);
    if (j.contains("sensorsBackend")) c.sensorsBackend = j.value("sensorsBackend", c.sensorsBackend);

    if (j.contains("profilesDir"))  c.profilesDir = j.value("profilesDir", c.profilesDir);
    if (j.contains("profile"))      c.profileName = j.value("profile", c.profileName);
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
}

DaemonConfig loadDaemonConfig(const std::string& configPath, std::string* err) {
    DaemonConfig cfg;

    if (!configPath.empty()) {
        try {
            fs::path p = expandUserPath(configPath);
            if (fs::exists(p)) {
                auto s = readFile(p);
                auto j = json::parse(s);
                json_to_cfg(j, cfg);
                cfg.configFile = p.string();
            } else if (err) {
                *err = "config file not found: " + p.string();
            }
        } catch (const std::exception& ex) {
            if (err) *err = std::string("failed to load config: ") + ex.what();
        }
    }

    apply_env_overrides(cfg);

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
