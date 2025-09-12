#include "RpcServer.h"
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <poll.h>
#include <cstring>
#include <cstdio>

RpcServer::RpcServer(const std::string& sockPath, Handler h)
: path_(sockPath), handler_(std::move(h)) {}

RpcServer::~RpcServer() { stop(); }

bool RpcServer::start() {
    ::unlink(path_.c_str());
    srv_ = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (srv_ < 0) return false;
    sockaddr_un addr{}; addr.sun_family = AF_UNIX;
    std::strncpy(addr.sun_path, path_.c_str(), sizeof(addr.sun_path)-1);
    if (::bind(srv_, (sockaddr*)&addr, sizeof(addr)) != 0) return false;
    if (::listen(srv_, 4) != 0) return false;
    return true;
}

void RpcServer::stop() {
    if (cli_!=-1) { ::close(cli_); cli_=-1; }
    if (srv_!=-1) { ::close(srv_); srv_=-1; ::unlink(path_.c_str()); }
}

void RpcServer::pump(int timeoutMs) {
    // Accept new client if none
    if (cli_ == -1) {
        pollfd p{srv_, POLLIN, 0};
        int pr = ::poll(&p, 1, timeoutMs);
        if (pr > 0 && (p.revents & POLLIN)) {
            cli_ = ::accept(srv_, nullptr, nullptr);
        }
        return;
    }
    // Read one line from client
    pollfd p{cli_, POLLIN, 0};
    int pr = ::poll(&p, 1, timeoutMs);
    if (pr <= 0) return;
    char ch; std::string line;
    while (true) {
        ssize_t n = ::read(cli_, &ch, 1);
        if (n <= 0) { ::close(cli_); cli_=-1; return; }
        if (ch=='\n') break;
        line.push_back(ch);
    }
    std::string resp = handler_(line);
    resp.push_back('\n');
    const char* data = resp.data(); size_t left = resp.size();
    while (left > 0) {
        ssize_t n = ::write(cli_, data, left);
        if (n <= 0) { ::close(cli_); cli_=-1; break; }
        left -= (size_t)n; data += n;
    }
}
