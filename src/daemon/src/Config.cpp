/*
 * Linux Fan Control — Config (implementation)
 * - JSON load/save using lightweight helper
 * (c) 2025 LinuxFanControl contributors
 */
#include "Config.hpp"
#include "JsonLite.hpp"

#include <fstream>
#include <sstream>
#include <filesystem>

using namespace std;

namespace lfc {

    DaemonConfig Config::Defaults() { return DaemonConfig{}; }

    static bool read_file(const string& path, string& out, string& err) {
        if (!std::filesystem::exists(path)) { err = "config not found"; return false; }
        ifstream f(path);
        if (!f) { err = "open failed"; return false; }
        std::ostringstream ss; ss << f.rdbuf();
        out = ss.str();
        return true;
    }

    static string get_str(const jsonlite::Value* v, const string& def) {
        return (v && v->isStr()) ? v->asStr() : def;
    }
    static bool get_bool(const jsonlite::Value* v, bool def) {
        if (!v) return def;
        if (v->isBool()) return v->asBool();
        if (v->isNum())  return v->asNum() != 0.0;
        return def;
    }
    static int get_int(const jsonlite::Value* v, int def) {
        if (!v) return def;
        if (v->isNum()) return static_cast<int>(v->asNum());
        return def;
    }
    static std::size_t get_size(const jsonlite::Value* v, std::size_t def) {
        if (!v) return def;
        if (v->isNum()) return static_cast<std::size_t>(v->asNum());
        return def;
    }

    bool Config::Load(const string& path, DaemonConfig& out, string& err) {
        string txt;
        if (!read_file(path, txt, err)) return false;

        jsonlite::Value root;
        if (!jsonlite::parse(txt, root, err)) return false;

        DaemonConfig cfg = Defaults();

        if (auto jlog = jsonlite::objGet(root, "log")) {
            cfg.log.file        = get_str(jsonlite::objGet(*jlog, "file"),        cfg.log.file);
            cfg.log.maxBytes    = get_size(jsonlite::objGet(*jlog, "maxBytes"),   cfg.log.maxBytes);
            cfg.log.rotateCount = get_int(jsonlite::objGet(*jlog, "rotateCount"), cfg.log.rotateCount);
            cfg.log.debug       = get_bool(jsonlite::objGet(*jlog, "debug"),      cfg.log.debug);
        }
        if (auto jr = jsonlite::objGet(root, "rpc")) {
            cfg.rpc.host = get_str(jsonlite::objGet(*jr, "host"), cfg.rpc.host);
            cfg.rpc.port = get_int(jsonlite::objGet(*jr, "port"), cfg.rpc.port);
        }
        if (auto js = jsonlite::objGet(root, "shm")) {
            cfg.shm.path = get_str(jsonlite::objGet(*js, "path"), cfg.shm.path);
        }
        if (auto jp = jsonlite::objGet(root, "profiles")) {
            cfg.profiles.dir     = get_str(jsonlite::objGet(*jp, "dir"),     cfg.profiles.dir);
            cfg.profiles.active  = get_str(jsonlite::objGet(*jp, "active"),  cfg.profiles.active);
            cfg.profiles.backups = get_bool(jsonlite::objGet(*jp, "backups"), cfg.profiles.backups);
        }
        cfg.pidFile = get_str(jsonlite::objGet(root, "pidFile"), cfg.pidFile);

        out = cfg;
        return true;
    }

    // manual pretty JSON (4-space indent) from DaemonConfig
    static inline void put_indent(std::ostream& os, int n) {
        for (int i = 0; i < n; ++i) os.put(' ');
    }
    static std::string jstr(const std::string& s) {
        std::string out; out.reserve(s.size() + 2);
        out.push_back('"');
        for (char c : s) {
            switch (c) {
                case '\\': out += "\\\\"; break;
                case '\"': out += "\\\""; break;
                case '\n': out += "\\n"; break;
                case '\r': out += "\\r"; break;
                case '\t': out += "\\t"; break;
                default:   out.push_back(c); break;
            }
        }
        out.push_back('"');
        return out;
    }

    bool Config::Save(const string& path, const DaemonConfig& in, string& err) {
        std::error_code ec;
        std::filesystem::create_directories(std::filesystem::path(path).parent_path(), ec);

        std::ofstream f(path, std::ios::trunc);
        if (!f) { err = "open for write failed"; return false; }

        int i = 0;
        f << "{\n";
        i += 4;

        // log
        put_indent(f, i); f << "\"log\": {\n";
        i += 4;
        put_indent(f, i); f << "\"file\": "        << jstr(in.log.file) << ",\n";
        put_indent(f, i); f << "\"maxBytes\": "    << static_cast<unsigned long long>(in.log.maxBytes) << ",\n";
        put_indent(f, i); f << "\"rotateCount\": " << in.log.rotateCount << ",\n";
        put_indent(f, i); f << "\"debug\": "       << (in.log.debug ? "true" : "false") << "\n";
        i -= 4;
        put_indent(f, i); f << "},\n";

        // rpc
        put_indent(f, i); f << "\"rpc\": {\n";
        i += 4;
        put_indent(f, i); f << "\"host\": " << jstr(in.rpc.host) << ",\n";
        put_indent(f, i); f << "\"port\": " << in.rpc.port << "\n";
        i -= 4;
        put_indent(f, i); f << "},\n";

        // shm
        put_indent(f, i); f << "\"shm\": {\n";
        i += 4;
        put_indent(f, i); f << "\"path\": " << jstr(in.shm.path) << "\n";
        i -= 4;
        put_indent(f, i); f << "},\n";

        // profiles
        put_indent(f, i); f << "\"profiles\": {\n";
        i += 4;
        put_indent(f, i); f << "\"dir\": "     << jstr(in.profiles.dir) << ",\n";
        put_indent(f, i); f << "\"active\": "  << jstr(in.profiles.active) << ",\n";
        put_indent(f, i); f << "\"backups\": " << (in.profiles.backups ? "true" : "false") << "\n";
        i -= 4;
        put_indent(f, i); f << "},\n";

        // pidFile
        put_indent(f, i); f << "\"pidFile\": " << jstr(in.pidFile) << "\n";

        i -= 4;
        f << "}\n";

        return static_cast<bool>(f);
    }

} // namespace lfc
