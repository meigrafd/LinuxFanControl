/*
 * Linux Fan Control — RPC TCP Server (header)
 * Minimal newline-delimited JSON-RPC 2.0 over TCP.
 * (c) 2025 LinuxFanControl contributors
 */
#pragma once

#include <atomic>
#include <string>
#include <thread>
#include <unordered_map>

namespace lfc {

class Daemon;
class CommandRegistry; // forward declare to avoid heavy include

class RpcTcpServer {
public:
    RpcTcpServer(Daemon& owner, const std::string& host, unsigned short port, bool verbose);
    ~RpcTcpServer();

    bool start(CommandRegistry* reg);
    void stop();

private:
    struct Client {
        std::string acc; // receive accumulator (per-connection)
    };

    void loop_();
    bool handleLine_(int fd, const std::string& line, std::string& outJson);

private:
    Daemon& owner_;
    std::string host_;
    unsigned short port_{0};
    bool verbose_{false};

    std::atomic<bool> running_{false};
    int listenFd_{-1};
    std::thread thr_;

    std::unordered_map<int, Client> clients_;

    CommandRegistry* reg_{nullptr};
};

} // namespace lfc
