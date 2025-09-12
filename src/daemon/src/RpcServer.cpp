#include "RpcServer.h"
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <cstring>
#include <iostream>

RpcServer::~RpcServer() { stop(); }

bool RpcServer::start(const std::string& sockPath, Handler h) {
    stop();
    path_ = sockPath;
    handler_ = std::move(h);

    fd_ = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd_ < 0) { perror("socket"); return false; }

    ::unlink(path_.c_str());
    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    std::snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", path_.c_str());

    if (::bind(fd_, (sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind"); ::close(fd_); fd_ = -1; return false;
    }
    if (::listen(fd_, 8) < 0) {
        perror("listen"); ::close(fd_); fd_ = -1; return false;
    }
    running_ = true;
    th_ = std::thread(&RpcServer::loop, this);
    return true;
}

void RpcServer::stop() {
    running_ = false;
    if (fd_ >= 0) {
        ::shutdown(fd_, SHUT_RDWR);
        ::close(fd_);
        fd_ = -1;
    }
    if (th_.joinable()) th_.join();
    if (!path_.empty()) ::unlink(path_.c_str());
}

void RpcServer::loop() {
    while (running_) {
        int cfd = ::accept(fd_, nullptr, nullptr);
        if (cfd < 0) {
            if (running_) perror("accept");
            continue;
        }
        // Serve this client synchronously; short sessions
        FILE* in = ::fdopen(cfd, "r+");
        if (!in) { ::close(cfd); continue; }
        char* line = nullptr; size_t cap = 0;
        ssize_t n;
        while (running_ && (n = ::getline(&line, &cap, in)) > 0) {
            if (n && line[n-1]=='\n') line[n-1] = 0;
            std::string req(line ? line : "");
            std::string resp = handler_ ? handler_(req) : std::string("{\"ok\":false}\n");
            ::fprintf(in, "%s", resp.c_str());
            ::fflush(in);
        }
        if (line) ::free(line);
        ::fclose(in); // closes cfd
    }
}
