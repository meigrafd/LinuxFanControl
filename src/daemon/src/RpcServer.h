#pragma once
#include <string>
#include <functional>

/* Minimal UNIX socket server (newline-delimited JSON).
 * For each line: std::string handle(std::string line) -> response line.
 */
class RpcServer {
public:
    using Handler = std::function<std::string(const std::string&)>;
    RpcServer(const std::string& sockPath, Handler h);
    ~RpcServer();

    bool start();
    void stop();
    void pump(int timeoutMs); // accept + read/write

private:
    int srv_ = -1;
    int cli_ = -1;
    std::string path_;
    Handler handler_;
};
