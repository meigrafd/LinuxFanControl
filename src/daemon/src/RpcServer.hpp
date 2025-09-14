#pragma once
/*
 * Minimal HTTP JSON-RPC 2.0 server with batch support.
 * Transport: HTTP/1.1 POST /rpc application/json
 * (c) 2025 LinuxFanControl contributors
 */
#include <string>
#include <thread>
#include <atomic>

class Daemon;

class RpcServer {
public:
    RpcServer(Daemon& d, const std::string& host, uint16_t port, bool debug);
    ~RpcServer();

    bool start(std::string& err);
    void stop();

private:
    void runAcceptLoop();
    void handleClient(int fd);
    bool readHttp(int fd, std::string& method, std::string& path, int& contentLen, std::string& body);
    std::string handleJsonRpc(const std::string& json, bool& hasAnyResponse);
    std::string makeHttpResponse(int code, const std::string& body, const char* ctype="application/json");
    static std::string buildEnvelope(const std::string& idJson, const std::string& innerObjectJson);

private:
    Daemon& daemon_;
    std::string host_;
    uint16_t port_;
    bool debug_;

    int listenfd_ = -1;
    std::thread thr_;
    std::atomic<bool> running_{false};
};
