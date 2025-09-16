/*
 * Linux Fan Control — main
 * (c) 2025 LinuxFanControl contributors
 */
#include "Daemon.hpp"
#include "Config.hpp"
#include "Log.hpp"

#include <iostream>
#include <string>

using namespace lfc;

static std::string normalize_profile_name(const std::string& in) {
    if (in.empty()) return in;
    if (in.size() > 5 && in.substr(in.size() - 5) == ".json") return in;
    return in + ".json";
}

int main(int, char**) {
    std::string err;
    DaemonConfig cfg = loadDaemonConfig(&err);
    if (!err.empty()) {
        std::cerr << "[warn] config load: " << err << "\n";
    }

    Daemon d;
    if (!d.init(cfg, cfg.debug, cfg.configFile, cfg.foreground)) {
        std::cerr << "[error] daemon init failed\n";
        return 1;
    }

    if (!cfg.profileName.empty()) {
        std::string full = cfg.profilesDir + "/" + normalize_profile_name(cfg.profileName);
        (void)d.applyProfileFile(full);
    }

    LFC_LOGI("lfcd started on %s:%d", cfg.host.c_str(), cfg.port);
    return 0;
}
