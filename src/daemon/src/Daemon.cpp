#include "Daemon.h"
#include <sensors/sensors.h>
#include <algorithm>
#include <sstream>
#include <fstream>
#include <cctype>

bool Daemon::init() {
    // libsensors init (not strictly used yet, but ready)
    if (sensors_init(nullptr) != 0) {
        // continue anyway; we rely on sysfs for now
    }
    temps_ = Hwmon::discoverTemps();
    pwms_  = Hwmon::discoverPwms();
    engine_.setChannels(chans_);
    return true;
}

void Daemon::shutdown() {
    engine_.stop();
    sensors_cleanup();
}

void Daemon::pump() { /* reserved for future periodic tasks */ }

std::string Daemon::jsonEscape(const std::string& s) {
    std::string o; o.reserve(s.size()+8);
    for (char c : s) {
        switch(c){
            case '\"': o += "\\\""; break;
            case '\\': o += "\\\\"; break;
            case '\n': o += "\\n"; break;
            case '\r': o += "\\r"; break;
            case '\t': o += "\\t"; break;
            default:   o += c; break;
        }
    }
    return o;
}
std::string Daemon::ok(const std::string& payload) { return std::string("{\"result\":") + payload + "}"; }
std::string Daemon::err(const std::string& msg) { return std::string("{\"error\":\"") + jsonEscape(msg) + "\"}"; }

static std::string toLower(std::string s){ std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){return std::tolower(c);}); return s; }

bool Daemon::findString(const std::string& b, const char* key, std::string& out) {
    // naive: search for "key":"...". not a general JSON parser (good enough for our params).
    std::string pat = std::string("\"") + key + "\":";
    auto pos = b.find(pat);
    if (pos == std::string::npos) return false;
    pos += pat.size();
    while (pos < b.size() && std::isspace((unsigned char)b[pos])) ++pos;
    if (pos >= b.size() || b[pos] != '\"') return false;
    ++pos;
    std::string val;
    while (pos < b.size() && b[pos] != '\"') { if (b[pos]=='\\' && pos+1<b.size()) ++pos; val.push_back(b[pos++]); }
    out = val; return true;
}
bool Daemon::findDouble(const std::string& b, const char* key, double& out) {
    std::string pat = std::string("\"") + key + "\":";
    auto pos = b.find(pat);
    if (pos == std::string::npos) return false;
    pos += pat.size();
    while (pos < b.size() && (std::isspace((unsigned char)b[pos]) || b[pos]==',')) ++pos;
    std::string num; bool seen=false;
    while (pos < b.size() && (std::isdigit((unsigned char)b[pos]) || b[pos]=='.' || b[pos]=='-' || b[pos]=='+')) {
        num.push_back(b[pos++]); seen=true;
    }
    if (!seen) return false;
    try { out = std::stod(num); return true; } catch (...) { return false; }
}

std::string Daemon::rpc_enumerate() {
    std::ostringstream ss;
    ss << "{\"temps\":[";
    for (size_t i=0;i<temps_.size();++i) {
        const auto& t = temps_[i];
        ss << "{\"label\":\"" << jsonEscape(t.label) << "\",\"path\":\"" << jsonEscape(t.path)
        << "\",\"type\":\"" << jsonEscape(t.type) << "\"}";
        if (i+1<temps_.size()) ss << ",";
    }
    ss << "],\"pwms\":[";
    for (size_t i=0;i<pwms_.size();++i) {
        const auto& p = pwms_[i];
        ss << "{\"label\":\"" << jsonEscape(p.label) << "\",\"pwm_path\":\"" << jsonEscape(p.pwm_path)
        << "\",\"enable_path\":\"" << jsonEscape(p.enable_path) << "\",\"tach_path\":\"" << jsonEscape(p.tach_path)
        << "\",\"writable\":" << (p.writable ? "true" : "false") << "}";
        if (i+1<pwms_.size()) ss << ",";
    }
    ss << "]}";
    return ok(ss.str());
}

std::string Daemon::rpc_listChannels() {
    std::lock_guard<std::mutex> lk(mu_);
    auto snap = engine_.snapshot();
    std::ostringstream ss;
    ss << "[";
    for (size_t i=0;i<snap.size();++i) {
        const auto& c = snap[i];
        ss << "{\"id\":\"" << jsonEscape(c.id) << "\",\"name\":\"" << jsonEscape(c.name)
        << "\",\"sensor_path\":\"" << jsonEscape(c.sensor_path) << "\",\"pwm_path\":\"" << jsonEscape(c.pwm_path)
        << "\",\"enable_path\":\"" << jsonEscape(c.enable_path) << "\",\"mode\":\"" << jsonEscape(c.mode)
        << "\",\"manual_pct\":" << c.manual_pct << ",\"last_temp\":" << c.last_temp
        << ",\"last_out\":" << c.last_out << "}";
        if (i+1<snap.size()) ss << ",";
    }
    ss << "]";
    return ok(ss.str());
}

std::string Daemon::rpc_createChannel(const std::string& name, const std::string& sensor, const std::string& pwm, const std::string& enable) {
    std::lock_guard<std::mutex> lk(mu_);
    Channel c;
    c.id = "ch" + std::to_string(nextId_++);
    c.name = name.empty() ? c.id : name;
    c.sensor_path = sensor;
    c.pwm_path = pwm;
    c.enable_path = enable;
    chans_.push_back(c);
    engine_.setChannels(chans_);
    return ok("\"ok\"");
}

std::string Daemon::rpc_deleteChannel(const std::string& id) {
    std::lock_guard<std::mutex> lk(mu_);
    auto it = std::remove_if(chans_.begin(), chans_.end(), [&](const Channel& c){return c.id==id;});
    if (it == chans_.end()) return err("not found");
    chans_.erase(it, chans_.end());
    engine_.setChannels(chans_);
    return ok("\"ok\"");
}

std::string Daemon::rpc_setChannelMode(const std::string& id, const std::string& mode) {
    engine_.updateChannelMode(id, mode);
    return ok("\"ok\"");
}
std::string Daemon::rpc_setChannelManual(const std::string& id, double pct) {
    engine_.updateChannelManual(id, pct);
    return ok("\"ok\"");
}
std::string Daemon::rpc_setChannelCurve(const std::string& id, const std::vector<CurvePoint>& pts) {
    engine_.updateChannelCurve(id, pts);
    return ok("\"ok\"");
}
std::string Daemon::rpc_setChannelHystTau(const std::string& id, double hyst, double tau) {
    engine_.updateChannelHystTau(id, hyst, tau);
    return ok("\"ok\"");
}
std::string Daemon::rpc_engineStart() {
    engine_.start(); return ok("\"ok\"");
}
std::string Daemon::rpc_engineStop() {
    engine_.stop(); return ok("\"ok\"");
}

std::string Daemon::rpc_detectCoupling(double hold_s, double /*min_dC*/, int /*rpm_delta*/) {
    // Minimal stub: do nothing, return empty object.
    (void)hold_s;
    return ok("{}");
}

std::string Daemon::handleRequest(const std::string& line) {
    // Expect {"id":..., "method":"...", "params":{...}}
    std::string m;
    if (!findString(line, "method", m)) return err("no method");
    std::string body = line; // params body can be searched directly

    if (m == "enumerate") {
        temps_ = Hwmon::discoverTemps();
        pwms_  = Hwmon::discoverPwms();
        return rpc_enumerate();
    }
    if (m == "listChannels") {
        return rpc_listChannels();
    }
    if (m == "createChannel") {
        std::string name, sensor, pwm, enable;
        findString(body, "name", name);
        findString(body, "sensor_path", sensor);
        findString(body, "pwm_path", pwm);
        findString(body, "enable_path", enable);
        if (pwm.empty() || sensor.empty()) return err("missing args");
        return rpc_createChannel(name, sensor, pwm, enable);
    }
    if (m == "deleteChannel") {
        std::string id; if (!findString(body, "id", id)) return err("missing id");
        return rpc_deleteChannel(id);
    }
    if (m == "setChannelMode") {
        std::string id, mode;
        if (!findString(body, "id", id) || !findString(body, "mode", mode)) return err("missing args");
        return rpc_setChannelMode(id, mode);
    }
    if (m == "setChannelManual") {
        std::string id; double pct=0.0;
        if (!findString(body, "id", id) || !findDouble(body, "pct", pct)) return err("missing args");
        return rpc_setChannelManual(id, pct);
    }
    if (m == "setChannelCurve") {
        std::string id; if (!findString(body, "id", id)) return err("missing id");
        // parse array of pairs: "curve":[[x,y],...]
        std::vector<CurvePoint> pts;
        auto pos = body.find("\"curve\":");
        if (pos != std::string::npos) {
            pos = body.find('[', pos);
            if (pos != std::string::npos) {
                ++pos;
                while (pos < body.size()) {
                    while (pos<body.size() && std::isspace((unsigned char)body[pos])) ++pos;
                    if (pos>=body.size() || body[pos]==']') break;
                    if (body[pos] != '[') { ++pos; continue; }
                    ++pos;
                    // read x
                    std::string sx, sy;
                    while (pos<body.size() && (std::isdigit((unsigned char)body[pos])||body[pos]=='.'||body[pos]=='-'||body[pos]=='+')) sx.push_back(body[pos++]);
                    while (pos<body.size() && body[pos]!=',' && body[pos]!=']') ++pos;
                    if (pos<body.size() && body[pos]==',') ++pos;
                    while (pos<body.size() && (std::isdigit((unsigned char)body[pos])||body[pos]=='.'||body[pos]=='-'||body[pos]=='+')) sy.push_back(body[pos++]);
                    // skip to ']'
                    while (pos<body.size() && body[pos]!=']') ++pos;
                    if (pos<body.size() && body[pos]==']') ++pos;
                    try {
                        pts.push_back(CurvePoint{std::stod(sx), std::stod(sy)});
                    } catch (...) {}
                    // skip comma
                    while (pos<body.size() && body[pos]!=']' && body[pos]!= '[') ++pos;
                    if (pos<body.size() && body[pos]==']') { ++pos; break; }
                }
            }
        }
        return rpc_setChannelCurve(id, pts);
    }
    if (m == "setChannelHystTau") {
        std::string id; double hyst=0.5, tau=2.0;
        if (!findString(body, "id", id)) return err("missing id");
        findDouble(body, "hyst", hyst);
        findDouble(body, "tau",  tau);
        return rpc_setChannelHystTau(id, hyst, tau);
    }
    if (m == "engineStart") return rpc_engineStart();
    if (m == "engineStop")  return rpc_engineStop();
    if (m == "detectCoupling") {
        double hold=10.0, dC=1.0; int rpm=80;
        findDouble(body, "hold_s", hold);
        findDouble(body, "min_delta_c", dC);
        findDouble(body, "rpm_delta_threshold", *(double*)&rpm); // dirty but works for ints in our helper
        return rpc_detectCoupling(hold, dC, rpm);
    }
    return err("unknown method");
}
