#include "Daemon.h"
#include <chrono>
#include <thread>
#include <cstdio>
#include <cstring>
#include <sstream>
#include <unordered_map>
#include <cctype>
#include <limits>

using nlohmann::json;

static json j_error(int code, const char* msg, const json& id = nullptr) {
    return json{{"jsonrpc","2.0"},{"error",json{{"code",code},{"message",msg}}},{"id",id}};
}
static json j_result(const json& res, const json& id) {
    return json{{"jsonrpc","2.0"},{"result",res},{"id",id}};
}

bool Daemon::init() {
    // Initial scan
    temps_ = hw_.discoverTemps();
    pwms_  = hw_.discoverPwms();

    // Start socket server; pass handler that expects JSON-RPC batch lines
    if (!rpc_.start("/tmp/lfcd.sock", [this](const std::string& line){ return this->onCommand(line); })) {
        return false;
    }
    return true;
}

void Daemon::shutdown() {
    rpc_.stop();
}

void Daemon::pumpOnce(int pollMs) {
    // Accept loop runs inside RpcServer; here we can rescan periodically later if needed.
    std::this_thread::sleep_for(std::chrono::milliseconds(pollMs));
}

std::string Daemon::onCommand(const std::string& jsonLine) {
    // Only JSON-RPC 2.0 batch supported (array of request objects).
    // We also accept a single JSON object as a degenerate batch of size 1 for convenience.
    std::string in = jsonLine;
    // Trim leading spaces just in case
    auto ltrim = [](std::string& s){
        size_t i=0; while (i<s.size() && std::isspace(static_cast<unsigned char>(s[i]))) ++i;
        if (i) s.erase(0,i);
    };
        ltrim(in);
        if (in.empty()) return (j_error(-32600, "invalid request")).dump() + "\n";
        return handleJsonRpcBatch(in);
}

std::string Daemon::handleJsonRpcBatch(const std::string& jsonText) {
    json parsed;
    try {
        parsed = json::parse(jsonText);
    } catch (...) {
        return (j_error(-32700, "parse error")).dump() + "\n";
    }

    // If single object, treat as batch of 1
    if (parsed.is_object()) {
        json out = json::array();
        auto r = jsonRpcCall(parsed);
        if (!r.is_null()) out.push_back(std::move(r));
        return out.dump() + "\n";
    }

    // Batch required
    if (!parsed.is_array() || parsed.empty()) {
        return (j_error(-32600, "invalid request")).dump() + "\n";
    }

    json out = json::array();
    for (auto& it : parsed) {
        if (!it.is_object()) { out.push_back(j_error(-32600, "invalid request")); continue; }
        auto r = jsonRpcCall(it);
        if (!r.is_null()) out.push_back(std::move(r)); // notifications -> no response
    }
    return out.dump() + "\n";
}

json Daemon::jsonRpcCall(const json& req) {
    // Validate envelope
    if (!req.contains("jsonrpc") || req["jsonrpc"] != "2.0" || !req.contains("method") || !req["method"].is_string()) {
        return j_error(-32600, "invalid request", req.value("id", nullptr));
    }
    const json id  = req.contains("id") ? req["id"] : nullptr; // null => notification
    const auto& m  = req["method"];
    const auto& p  = req.value("params", json::object());
    const bool hasId = !id.is_null();
    auto err = [&](int code, const char* msg){ return hasId ? j_error(code,msg,id) : json(); };

    const std::string method = m.get<std::string>();

    try {
        // ---------- Queries ----------
        if (method == "enumerate") {
            std::lock_guard<std::mutex> lk(mtx_);
            // Reuse legacy JSON builders (they already compose arrays/objects)
            json res = json::parse(jsonEnumerate());
            return hasId ? j_result(res, id) : json();
        }
        if (method == "listChannels") {
            json res = json::parse(jsonListChannels());
            return hasId ? j_result(res["channels"], id) : json();
        }
        if (method == "LIST_TEMPS") { // optional helper
            std::lock_guard<std::mutex> lk(mtx_);
            json res = json::parse(jsonListTemps());
            return hasId ? j_result(res["temps"], id) : json();
        }
        if (method == "LIST_PWMS") {  // optional helper
            std::lock_guard<std::mutex> lk(mtx_);
            json res = json::parse(jsonListPwms());
            return hasId ? j_result(res["pwms"], id) : json();
        }

        // ---------- Mutations ----------
        if (method == "createChannel") {
            if (!p.is_object()) return err(-32602,"invalid params");
            auto name   = p.value("name", std::string());
            auto sensor = p.value("sensor", std::string());
            auto pwm    = p.value("pwm", std::string());
            if (name.empty() || pwm.empty()) return err(-32602,"missing name or pwm");
            int cid = engine_.createChannel(name, sensor, pwm);
            return hasId ? j_result(json{{"id",cid}}, id) : json();
        }
        if (method == "deleteChannel") {
            if (!p.is_object()) return err(-32602,"invalid params");
            int idv = p.value("id", 0);
            bool ok = idv>0 && engine_.deleteChannel(idv);
            return hasId ? j_result(json{{"ok",ok}}, id) : json();
        }
        if (method == "setChannelMode") {
            if (!p.is_object()) return err(-32602,"invalid params");
            int idv = p.value("id", 0);
            auto mode = p.value("mode", std::string());
            bool ok = idv>0 && engine_.setMode(idv, mode);
            return hasId ? j_result(json{{"ok",ok}}, id) : json();
        }
        if (method == "setChannelManual") {
            if (!p.is_object()) return err(-32602,"invalid params");
            int idv = p.value("id", 0);
            double val = p.value("value", 0.0);
            bool ok = idv>0 && engine_.setManual(idv, val);
            return hasId ? j_result(json{{"ok",ok}}, id) : json();
        }
        if (method == "setChannelCurve") {
            if (!p.is_object()) return err(-32602,"invalid params");
            int idv = p.value("id", 0);
            std::vector<std::pair<double,double>> pts;
            if (p.contains("points") && p["points"].is_array()) {
                for (auto& v : p["points"]) {
                    if (v.is_array() && v.size()==2 && v[0].is_number() && v[1].is_number()) {
                        pts.emplace_back(v[0].get<double>(), v[1].get<double>());
                    } else if (v.is_object() && v.contains("x") && v.contains("y")) {
                        pts.emplace_back(v["x"].get<double>(), v["y"].get<double>());
                    }
                }
            }
            if (pts.size()<2) return err(-32602,"invalid curve");
            bool ok = idv>0 && engine_.setCurve(idv, pts);
            return hasId ? j_result(json{{"ok",ok}}, id) : json();
        }
        if (method == "setChannelHystTau") {
            if (!p.is_object()) return err(-32602,"invalid params");
            int idv = p.value("id", 0);
            double hyst = p.value("hyst", 0.0);
            double tau  = p.value("tau",  0.0);
            bool ok = idv>0 && engine_.setHystTau(idv, hyst, tau);
            return hasId ? j_result(json{{"ok",ok}}, id) : json();
        }
        if (method == "engineStart") { engine_.start(); return hasId ? j_result(json{{"ok",true}}, id) : json(); }
        if (method == "engineStop")  { engine_.stop();  return hasId ? j_result(json{{"ok",true}}, id) : json(); }
        if (method == "deleteCoupling") {
            if (!p.is_object()) return err(-32602,"invalid params");
            int idv = p.value("id", 0);
            bool ok = idv>0 && engine_.deleteCoupling(idv);
            return hasId ? j_result(json{{"ok",ok}}, id) : json();
        }

        // Unknown method
        return err(-32601, "method not found");
    } catch (...) {
        return err(-32603, "internal error");
    }
}

// ---------- Legacy JSON builders (reused to produce payloads quickly) ----------
std::string Daemon::jsonEscape(const std::string& s) {
    std::string out; out.reserve(s.size()+8);
    for (char c : s) {
        switch (c) {
            case '\\': out += "\\\\"; break;
            case '\"': out += "\\\""; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default: out += c; break;
        }
    }
    return out;
}

std::string Daemon::jsonListTemps() const {
    std::string j = "{\"ok\":true,\"temps\":[";
    bool first = true;
    for (auto& t : temps_) {
        if (!first) j += ","; first = false;
        j += "{";
        j += "\"name\":\""+jsonEscape(t.name)+"\",";
        j += "\"path\":\""+jsonEscape(t.path)+"\",";
        j += "\"type\":\""+jsonEscape(t.type)+"\"";
        j += "}";
    }
    j += "]}\n";
    return j;
}

std::string Daemon::jsonListPwms() const {
    std::string j = "{\"ok\":true,\"pwms\":[";
    bool first = true;
    for (auto& p : pwms_) {
        if (!first) j += ","; first = false;
        j += "{";
        j += "\"label\":\""+jsonEscape(p.label)+"\",";
        j += "\"pwm\":\""+jsonEscape(p.pwmPath)+"\",";
        j += "\"enable\":\""+jsonEscape(p.enablePath)+"\",";
        j += "\"tach\":\""+jsonEscape(p.tachPath)+"\"";
        j += "}";
    }
    j += "]}\n";
    return j;
}

std::string Daemon::jsonEnumerate() const {
    std::string j = "{\"ok\":true,";
    // temps
    j += "\"temps\":[";
    bool first = true;
    for (auto& t : temps_) {
        if (!first) j += ","; first = false;
        j += "{";
        j += "\"name\":\""+jsonEscape(t.name)+"\",";
        j += "\"path\":\""+jsonEscape(t.path)+"\",";
        j += "\"type\":\""+jsonEscape(t.type)+"\"";
        j += "}";
    }
    j += "],";
    // pwms
    j += "\"pwms\":[";
    first = true;
    for (auto& p : pwms_) {
        if (!first) j += ","; first = false;
        j += "{";
        j += "\"label\":\""+jsonEscape(p.label)+"\",";
        j += "\"pwm\":\""+jsonEscape(p.pwmPath)+"\",";
        j += "\"enable\":\""+jsonEscape(p.enablePath)+"\",";
        j += "\"tach\":\""+jsonEscape(p.tachPath)+"\"";
        j += "}";
    }
    j += "]}";
    j += "\n";
    return j;
}

std::string Daemon::jsonListChannels() const {
    const auto& chs = engine_.channels();
    std::string j = "{\"ok\":true,\"channels\":[";
    bool first = true;
    for (auto& c : chs) {
        if (!first) j += ","; first = false;
        j += "{";
        j += "\"id\":" + std::to_string(c.id) + ",";
        j += "\"name\":\"" + jsonEscape(c.name) + "\",";
        j += "\"sensor\":\"" + jsonEscape(c.sensorPath) + "\",";
        j += "\"pwm\":\"" + jsonEscape(c.pwmPath) + "\",";
        j += "\"mode\":\"" + jsonEscape(c.mode) + "\",";
        j += "\"manual\":" + std::to_string((int) c.manualValue) + ",";
        j += "\"hyst\":" + std::to_string(c.hystC) + ",";
        j += "\"tau\":"  + std::to_string(c.tauS)  + ",";
        j += "\"curve\":[";
        bool f2=true;
        for (auto& pt : c.curve) {
            if (!f2) j += ","; f2=false;
            j += "[" + std::to_string(pt.first) + "," + std::to_string(pt.second) + "]";
        }
        j += "]";
        j += "}";
    }
    j += "]}\n";
    return j;
}

std::vector<std::pair<double,double>> Daemon::parsePoints(const std::string& payload) {
    // Accept JSON-like: [[x,y],[x,y],...]
    std::vector<std::pair<double,double>> pts;
    const char* s = payload.c_str();
    const char* p = s;
    while ((p = std::strchr(p, '['))) {
        char* end1=nullptr; char* end2=nullptr;
        double x = std::strtod(p+1, &end1);
        if (!end1 || *end1!=',') { ++p; continue; }
        double y = std::strtod(end1+1, &end2);
        if (!end2) { ++p; continue; }
        const char* close = std::strchr(end2, ']');
        if (!close) { ++p; continue; }
        pts.emplace_back(x,y);
        p = close + 1;
    }
    return pts;
}
