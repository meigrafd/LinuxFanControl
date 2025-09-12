#include "Daemon.h"
#include <chrono>
#include <thread>
#include <cstdio>

bool Daemon::init() {
    // Initial scan
    temps_ = hw_.discoverTemps();
    pwms_  = hw_.discoverPwms();
    // Start socket server
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

std::string Daemon::onCommand(const std::string& line) {
    // Very small text protocol (one token per line); replies are JSON strings.
    if (line == "PING") {
        return "{\"ok\":true,\"pong\":1}\n";
    }
    if (line == "LIST_TEMPS") {
        std::lock_guard<std::mutex> lk(mtx_);
        return jsonListTemps();
    }
    if (line == "LIST_PWMS") {
        std::lock_guard<std::mutex> lk(mtx_);
        return jsonListPwms();
    }
    if (line == "RESCAN") {
        std::lock_guard<std::mutex> lk(mtx_);
        temps_ = hw_.discoverTemps();
        pwms_  = hw_.discoverPwms();
        return "{\"ok\":true,\"rescanned\":true}\n";
    }
    return "{\"ok\":false,\"error\":\"unknown_command\"}\n";
}

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
