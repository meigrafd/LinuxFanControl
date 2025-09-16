/*
 * Linux Fan Control — RPC TCP Server (header)
 * - Minimal JSON-RPC 2.0 over TCP (newline-delimited)
 * - Non-blocking accept + client handling
 * (c) 2025 LinuxFanControl contributors
 */
#pragma once
#include <string>
#include <thread>
#include <atomic>
#include <vector>
#include <mutex>

namespace lfc {

class Daemon;
class CommandRegistry;

class RpcTcpServer {
public:
    RpcTcpServer(Daemon& owner, const std::string& host, unsigned short port, bool verbose);
    ~RpcTcpServer();

    bool start(CommandRegistry* reg);
    void stop();

private:
    void threadMain();
    bool handleLine(int fd, const std::string& line, std::string& outJson);

private:
    Daemon& owner_;
    CommandRegistry* reg_{nullptr};
    std::string host_;
    unsigned short port_{0};
    bool verbose_{false};

    std::thread thr_;
    std::atomic<bool> running_{false};
    int listenFd_{-1};

    std::mutex mu_;
    std::vector<int> clients_;
};

} // namespace lfc
