// Daemon entry: init libsensors and start RPC server.
#include <sensors/sensors.h>
#include <iostream>
#include <csignal>
#include "Daemon.h"

static volatile std::sig_atomic_t g_stop = 0;
static void on_sigint(int) { g_stop = 1; }

int main(int argc, char** argv) {
    (void)argc; (void)argv;
    std::signal(SIGINT, on_sigint);
    std::signal(SIGTERM, on_sigint);

    int rc = sensors_init(nullptr);
    if (rc != 0) {
        std::cerr << "[lfcd] sensors_init failed: " << rc << std::endl;
    }

    Daemon d;
    if (!d.init()) {
        std::cerr << "[lfcd] init failed\n";
        sensors_cleanup();
        return 1;
    }

    std::cout << "[lfcd] listening on /tmp/lfcd.sock (Ctrl+C to stop)\n";
    while (!g_stop) {
        d.pumpOnce(250); // 250ms step (non-blocking accept)
    }
    d.shutdown();
    sensors_cleanup();
    std::cout << "[lfcd] stopped\n";
    return 0;
}
