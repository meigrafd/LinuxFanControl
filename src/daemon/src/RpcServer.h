#pragma once
// Very small line-based RPC over Unix Domain Socket. One request per line; response is a JSON string.

#include <string>
#include <functional>
#include <thread>
#include <atomic>

class RpcServer {
public:
    using Handler = std::function<std::string(const std::string&)>;

    RpcServer() = default;
    ~RpcServer();

    bool start(const std::string& sockPath, Handler h);
    void stop();

private:
    int fd_ = -1;
    std::thread th_;
    std::atomic<bool> running_{false};
    std::string path_;
    Handler handler_;

    void loop();
};
