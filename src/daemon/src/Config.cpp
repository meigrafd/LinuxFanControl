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

    static jsonlite::Value to_json(const DaemonConfig& in) {
        using jsonlite::Value;
        using jsonlite::Object;

        Value root(Object{});
        auto& o = root.mutObj();

        Value jlog(Object{});
        jlog.mutObj()["file"]        = Value(in.log.file);
        jlog.mutObj()["maxBytes"]    = Value(static_cast<double>(in.log.maxBytes));
        jlog.mutObj()["rotateCount"] = Value(static_cast<double>(in.log.rotateCount));
        jlog.mutObj()["debug"]       = Value(in.log.debug);
        o["log"] = jlog;

        Value jr(Object{});
        jr.mutObj()["host"] = Value(in.rpc.host);
        jr.mutObj()["port"] = Value(static_cast<double>(in.rpc.port));
        o["rpc"] = jr;

        Value js(Object{});
        js.mutObj()["path"] = Value(in.shm.path);
        o["shm"] = js;

        Value jp(Object{});
        jp.mutObj()["dir"]     = Value(in.profiles.dir);
        jp.mutObj()["active"]  = Value(in.profiles.active);
        jp.mutObj()["backups"] = Value(in.profiles.backups);
        o["profiles"] = jp;

        o["pidFile"] = Value(in.pidFile);

        return root;
    }

    bool Config::Save(const string& path, const DaemonConfig& in, string& err) {
        std::error_code ec;
        std::filesystem::create_directories(std::filesystem::path(path).parent_path(), ec);

        std::ofstream f(path, std::ios::trunc);
        if (!f) { err = "open for write failed"; return false; }

        auto root = to_json(in);
        f << jsonlite::stringify(root) << "\n";
        return static_cast<bool>(f);
    }

} // namespace lfc
