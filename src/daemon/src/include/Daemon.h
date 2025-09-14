// src/daemon/include/Daemon.h
#pragma once
#include <cstdint>
#include <memory>

namespace lfc {
    class DaemonImpl; // pimpl

    class Daemon {
    public:
        Daemon();
        ~Daemon();

        // returns false to indicate "do not enter run loop" on --help / --list-commands
        bool initFromArgs(int argc, char** argv);

        void pumpOnce(uint32_t ms);
        void shutdown();

    private:
        std::unique_ptr<DaemonImpl> impl_;
    };
} // namespace lfc
