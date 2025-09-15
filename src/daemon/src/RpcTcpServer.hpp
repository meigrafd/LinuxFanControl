#pragma once
#include <string>
#include <vector>
#include <atomic>
#include <thread>
#include <cstdint>

namespace lfc {

    class CommandRegistry;
    class Daemon;

    class RpcTcpServer {
    public:
        RpcTcpServer(Daemon& d, const std::string& host, std::uint16_t port, bool debug);
        ~RpcTcpServer();

        // Start using an existing command registry
        bool start(CommandRegistry* reg);
        void stop();
        // Non-blocking step to keep interface symmetrical with main loop
        void pumpOnce(int /*timeoutMs*/);

        std::vector<std::string> listMethods() const;

    private:
        bool readLine(int fd, std::string& out);
        void handleClient(int fd);
        void runAcceptLoop();
        std::string handleJsonRpc(const std::string& json, bool& hasAnyResponse);

    private:
        Daemon& daemon_;
        std::string host_;
        std::uint16_t port_{0};
        int listenfd_{-1};
        std::atomic<bool> running_{false};
        std::thread thr_;
        CommandRegistry* reg_{nullptr};
        bool debug_{false};
    };

} // namespace lfc
