#pragma once
/*
 * Minimal JSON-RPC 2.0 server over plain TCP.
 * Framing: newline-delimited JSON per request/response; batch allowed.
 * (c) 2025 LinuxFanControl contributors
 */
#include <string>
#include <thread>
#include <atomic>

class Daemon;

class RpcTcpServer {
public:
    RpcTcpServer(Daemon& d, const std::string& host, uint16_t port, bool debug);
    ~RpcTcpServer();

    bool start(std::string& err);
    void stop();

private:
    void runAcceptLoop();
    void handleClient(int fd);
    static bool readLine(int fd, std::string& out);
    std::string handleJsonRpc(const std::string& json, bool& hasAnyResponse);

private:
    Daemon& daemon_;
    std::string host_;
    uint16_t port_;
    bool debug_;

    int listenfd_ = -1;
    std::thread thr_;
    std::atomic<bool> running_{false};
};
