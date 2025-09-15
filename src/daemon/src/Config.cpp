#include "Config.hpp"
#include <fstream>
#include <sstream>
#include <filesystem>

using namespace std;

namespace lfc {

    DaemonConfig Config::Defaults() { return DaemonConfig{}; }

    static string trim(const string& s) {
        const char* ws = " \t\r\n";
        auto b = s.find_first_not_of(ws);
        auto e = s.find_last_not_of(ws);
        if (b == string::npos) return {};
        return s.substr(b, e - b + 1);
    }

    bool Config::Load(const string& path, DaemonConfig& out, string& err) {
        if (!std::filesystem::exists(path)) { err = "config not found"; return false; }
        ifstream f(path);
        if (!f) { err = "open failed"; return false; }
        // ultra-simple line parser: key=value
        // acceptable keys:
        // log.file, log.maxBytes, log.rotateCount, log.debug
        // rpc.host, rpc.port
        // shm.path
        // profiles.dir, profiles.active, profiles.backups
        // pidFile
        string line;
        DaemonConfig cfg = Defaults();
        while (getline(f, line)) {
            line = trim(line);
            if (line.empty() || line[0] == '#') continue;
            auto eq = line.find('=');
            if (eq == string::npos) continue;
            string k = trim(line.substr(0, eq));
            string v = trim(line.substr(eq + 1));

            auto as_bool  = [&](const string& s){ return s=="1"||s=="true"||s=="True"||s=="TRUE"; };
            auto as_u64   = [&](const string& s){ return static_cast<size_t>(strtoull(s.c_str(), nullptr, 10)); };
            auto as_int   = [&](const string& s){ return static_cast<int>(strtol(s.c_str(), nullptr, 10)); };

            if      (k=="log.file")         cfg.log.file = v;
            else if (k=="log.maxBytes")     cfg.log.maxBytes = as_u64(v);
            else if (k=="log.rotateCount")  cfg.log.rotateCount = as_int(v);
            else if (k=="log.debug")        cfg.log.debug = as_bool(v);

            else if (k=="rpc.host")         cfg.rpc.host = v;
            else if (k=="rpc.port")         cfg.rpc.port = as_int(v);

            else if (k=="shm.path")         cfg.shm.path = v;

            else if (k=="profiles.dir")     cfg.profiles.dir = v;
            else if (k=="profiles.active")  cfg.profiles.active = v;
            else if (k=="profiles.backups") cfg.profiles.backups = as_bool(v);

            else if (k=="pidFile")          cfg.pidFile = v;
        }
        out = cfg;
        return true;
    }

    bool Config::Save(const string& path, const DaemonConfig& in, string& err) {
        std::error_code ec;
        std::filesystem::create_directories(std::filesystem::path(path).parent_path(), ec);
        ofstream f(path, ios::trunc);
        if (!f) { err = "open for write failed"; return false; }
        f << "# LinuxFanControl daemon config (ini-like key=value)\n";
        f << "log.file="          << in.log.file          << "\n";
        f << "log.maxBytes="      << in.log.maxBytes      << "\n";
        f << "log.rotateCount="   << in.log.rotateCount   << "\n";
        f << "log.debug="         << (in.log.debug ? "true" : "false") << "\n";
        f << "rpc.host="          << in.rpc.host          << "\n";
        f << "rpc.port="          << in.rpc.port          << "\n";
        f << "shm.path="          << in.shm.path          << "\n";
        f << "profiles.dir="      << in.profiles.dir      << "\n";
        f << "profiles.active="   << in.profiles.active   << "\n";
        f << "profiles.backups="  << (in.profiles.backups ? "true" : "false") << "\n";
        f << "pidFile="           << in.pidFile           << "\n";
        return true;
    }

} // namespace lfc
