/*
 * Daemon configuration (loads/saves JSON file, creates defaults if missing).
 * (c) 2025 LinuxFanControl contributors
 */
#include "Config.hpp"
#include "JsonLite.hpp"
#include <sys/stat.h>
#include <unistd.h>
#include <fstream>
#include <sstream>
#include <cstdlib>

static bool isDir(const std::string& p) {
    struct stat st{}; return ::stat(p.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}
static bool mkDirAll(const std::string& p, mode_t mode=0755) {
    if (isDir(p)) return true;
    std::ostringstream cmd; cmd << "mkdir -p " << p;
    return ::system(cmd.str().c_str()) == 0;
}
static bool isFile(const std::string& p) {
    struct stat st{}; return ::stat(p.c_str(), &st) == 0 && S_ISREG(st.st_mode);
}
static std::string dirnameOf(const std::string& p) {
    auto pos = p.find_last_of('/');
    return (pos==std::string::npos) ? "." : p.substr(0,pos);
}

namespace cfg {

    std::string defaultConfigPath() {
        // Prefer /etc
        if (access("/etc/lfc", W_OK) == 0 || access("/etc", W_OK) == 0) return "/etc/lfc/daemon.json";
        const char* xdg = std::getenv("XDG_CONFIG_HOME");
        std::string base = xdg ? xdg : std::string(std::getenv("HOME") ? std::getenv("HOME") : "") + "/.config";
        return base + "/lfc/daemon.json";
    }

    static bool readFile(const std::string& p, std::string& out) {
        std::ifstream f(p); if (!f) return false;
        std::ostringstream o; o << f.rdbuf(); out = o.str(); return true;
    }
    static bool writeFile(const std::string& p, const std::string& s) {
        std::ofstream f(p); if (!f) return false;
        f << s; return (bool)f;
    }

    bool ensureDirs(const DaemonConfig& c, std::string& err) {
        if (!mkDirAll(dirnameOf(c.logfile))) { err="mkdir logfile dir failed"; return false; }
        if (!mkDirAll(dirnameOf(c.pidfile))) { err="mkdir pidfile dir failed"; return false; }
        if (!mkDirAll(c.profiles_dir))       { err="mkdir profiles dir failed"; return false; }
        if (!mkDirAll("/var/log/lfc"))       { /* ignore */ }
        if (!mkDirAll("/run/lfc"))           { /* ignore */ }
        return true;
    }

    bool loadOrCreate(DaemonConfig& out, std::string customPath, std::string& err) {
        std::string path = customPath.empty() ? defaultConfigPath() : customPath;
        out._path = path;

        if (!isFile(path)) {
            // ensure parent dir
            if (!mkDirAll(dirnameOf(path))) { err="mkdir config dir failed"; return false; }
            // seed default file
            jsonlite::Object o;
            o["logfile"]  = jsonlite::Value(out.logfile);
            o["pidfile"]  = jsonlite::Value(out.pidfile);
            o["log_size_bytes"] = jsonlite::Value((double)out.log_size_bytes);
            o["log_rotate"] = jsonlite::Value((double)out.log_rotate);
            o["debug"] = jsonlite::Value(out.debug);
            o["profiles_dir"] = jsonlite::Value(out.profiles_dir);
            o["active_profile"] = jsonlite::Value(out.active_profile);
            o["profiles_backup"] = jsonlite::Value(out.profiles_backup);
            o["rpc_host"] = jsonlite::Value(out.rpc_host);
            o["rpc_port"] = jsonlite::Value((double)out.rpc_port);
            o["shm_path"] = jsonlite::Value(out.shm_path);
            std::string s = jsonlite::stringify(jsonlite::Value(std::move(o)));
            if (!writeFile(path, s)) { err="write default config failed"; return false; }
        }

        std::string data;
        if (!readFile(path, data)) { err="read config failed"; return false; }
        jsonlite::Value v; std::string perr;
        if (!jsonlite::parse(data, v, perr) || !v.isObject()) { err="parse config failed"; return false; }
        auto* O = &v;
        auto Gs=[&](const char* k, std::string& dst) { auto* x=jsonlite::objGet(*O,k); if (x&&x->isStr()) dst=x->asStr(); };
        auto Gi=[&](const char* k, std::size_t& dst){ auto* x=jsonlite::objGet(*O,k); if (x&&x->isNum()) dst=(std::size_t)x->asNum(); };
        auto Gd=[&](const char* k, uint16_t& dst){ auto* x=jsonlite::objGet(*O,k); if (x&&x->isNum()) dst=(uint16_t)x->asNum(); };
        auto Gb=[&](const char* k, bool& dst){ auto* x=jsonlite::objGet(*O,k); if (x&&x->isBool()) dst=x->asBool(); };

        Gs("logfile", out.logfile);
        Gs("pidfile", out.pidfile);
        Gi("log_size_bytes", out.log_size_bytes);
        { std::size_t tmp=out.log_rotate; Gi("log_rotate", tmp); out.log_rotate=(int)tmp; }
        Gb("debug", out.debug);
        Gs("profiles_dir", out.profiles_dir);
        Gs("active_profile", out.active_profile);
        Gb("profiles_backup", out.profiles_backup);
        Gs("rpc_host", out.rpc_host);
        Gd("rpc_port", out.rpc_port);
        Gs("shm_path", out.shm_path);

        if (!ensureDirs(out, err)) return false;
        return true;
    }

    bool save(const DaemonConfig& c, std::string& err) {
        jsonlite::Object o;
        o["logfile"]  = jsonlite::Value(c.logfile);
        o["pidfile"]  = jsonlite::Value(c.pidfile);
        o["log_size_bytes"] = jsonlite::Value((double)c.log_size_bytes);
        o["log_rotate"] = jsonlite::Value((double)c.log_rotate);
        o["debug"] = jsonlite::Value(c.debug);
        o["profiles_dir"] = jsonlite::Value(c.profiles_dir);
        o["active_profile"] = jsonlite::Value(c.active_profile);
        o["profiles_backup"] = jsonlite::Value(c.profiles_backup);
        o["rpc_host"] = jsonlite::Value(c.rpc_host);
        o["rpc_port"] = jsonlite::Value((double)c.rpc_port);
        o["shm_path"] = jsonlite::Value(c.shm_path);
        std::string s = jsonlite::stringify(jsonlite::Value(std::move(o)));
        if (!mkDirAll(dirnameOf(c._path))) { err="mkdir config dir failed"; return false; }
        std::ofstream f(c._path); if (!f){ err="open config for write failed"; return false; }
        f << s; return (bool)f;
    }

} // namespace cfg
