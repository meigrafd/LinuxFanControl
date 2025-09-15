#pragma once
#include <string>
#include <vector>
#include <memory>
#include <thread>
#include <atomic>
#include <cstdint>

namespace lfc {

    class CommandRegistry; // nur für API-Kompatibilität
    class Daemon;

    // TCP-basierter JSON-RPC Server (reine TCP-Variante, kein HTTP)
    // Header ist auf die vorhandene RpcTcpServer.cpp abgestimmt.
    class RpcTcpServer {
    public:
        // Voller Konstruktor (wird von Daemon verwendet)
        RpcTcpServer(Daemon& d, const std::string& host, std::uint16_t port, bool debug)
        : daemon_(d), host_(host), port_(port), debug_(debug) {}

        // Beibehaltene Kurzform für main.cpp-Kompatibilität (RpcServer rpc(daemon); rpc.start(host,port,debug);)
        explicit RpcTcpServer(Daemon& d) : daemon_(d) {}

        ~RpcTcpServer();

        // „neue“ Start-Variante (nutzt im .cpp host_/port_/debug_)
        bool start(std::string& err);

        // „kompatible“ Start-Variante wie bisher in manchen Aufrufern verwendet
        bool start(const std::string& host, int port, CommandRegistry* /*reg*/, bool debug=false) {
            (void)/*reg*/ debug; // Registry wird intern über Daemon erledigt
            host_  = host;
            port_  = static_cast<std::uint16_t>(port);
            debug_ = debug;
            std::string err;
            return start(err);
        }

        void stop();
        void pumpOnce(int /*timeoutMs*/); // non-blocking „step“ (Transport kapselt eigenen Thread)

        // optional: für introspection
        std::vector<std::string> listMethods() const { return {}; }

    private:
        // Thread-Helfer (implementiert in .cpp)
        void runAcceptLoop();
        bool readLine(int fd, std::string& out);
        void handleClient(int fd);
        std::string handleJsonRpc(const std::string& json, bool& hasAnyResponse);

    private:
        Daemon& daemon_;
        std::string  host_{"127.0.0.1"};
        std::uint16_t port_{8777};
        bool debug_{false};

        int listenfd_{-1};
        std::atomic<bool> running_{false};
        std::thread thr_;
    };

} // namespace lfc
