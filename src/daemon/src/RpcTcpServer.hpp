#pragma once
#include <string>
#include <vector>
#include <memory>

namespace lfc {

    class CommandRegistry;
    class Daemon;

    class RpcTcpServer {
    public:
        explicit RpcTcpServer(Daemon& d) : daemon_(d) {}
        bool start(const std::string& host, int port, CommandRegistry* /*reg*/) {
            (void)host; (void)port; /* Stub transport for now */ running_ = true; return true;
        }
        void stop() { running_ = false; }
        void pumpOnce(int /*timeoutMs*/) { /* transport stub */ }
        std::vector<std::string> listMethods() const { return {}; }

    private:
        Daemon& daemon_;
        bool running_{false};
    };

} // namespace lfc
