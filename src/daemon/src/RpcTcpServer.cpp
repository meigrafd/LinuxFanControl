/*
 * Linux Fan Control — RPC TCP Server (implementation)
 * - Minimal JSON-RPC 2.0 over TCP (newline-delimited)
 * - Uses select() to multiplex clients
 * (c) 2025 LinuxFanControl contributors
 */
#include "RpcTcpServer.hpp"
#include "include/CommandRegistry.h"
#include "Daemon.hpp"
#include "Log.hpp"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <cerrno>
#include <algorithm>
#include <string>
#include <sstream>

namespace lfc {

static int set_nonblock(int fd) {
    int fl = fcntl(fd, F_GETFL, 0);
    return fcntl(fd, F_SETFL, fl | O_NONBLOCK);
}

RpcTcpServer::RpcTcpServer(Daemon& owner, const std::string& host, unsigned short port, bool verbose)
    : owner_(owner), host_(host), port_(port), verbose_(verbose) {}

RpcTcpServer::~RpcTcpServer() {
    stop();
}

bool RpcTcpServer::start(CommandRegistry* reg) {
    reg_ = reg;
    listenFd_ = ::socket(AF_INET, SOCK_STREAM, 0);
    if (listenFd_ < 0) return false;

    int yes = 1;
    setsockopt(listenFd_, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port_);
    addr.sin_addr.s_addr = inet_addr(host_.c_str());
    if (::bind(listenFd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        ::close(listenFd_);
        listenFd_ = -1;
        return false;
    }
    if (::listen(listenFd_, 16) != 0) {
        ::close(listenFd_);
        listenFd_ = -1;
        return false;
    }
    set_nonblock(listenFd_);

    running_.store(true);
    thr_ = std::thread(&RpcTcpServer::threadMain, this);
    return true;
}

void RpcTcpServer::stop() {
    if (!running_.exchange(false)) return;
    if (listenFd_ >= 0) ::close(listenFd_);
    listenFd_ = -1;
    {
        std::lock_guard<std::mutex> lk(mu_);
        for (int fd : clients_) ::close(fd);
        clients_.clear();
    }
    if (thr_.joinable()) thr_.join();
}

bool RpcTcpServer::handleLine(int fd, const std::string& line, std::string& outJson) {
    // Expect JSON-RPC 2.0: {"jsonrpc":"2.0","id":...,"method":"...","params":...}
    auto findStr = [&](const char* key) -> std::string {
        std::string k = std::string("\"") + key + "\"";
        auto p = line.find(k);
        if (p == std::string::npos) return {};
        p = line.find(':', p);
        if (p == std::string::npos) return {};
        auto q = line.find('"', p + 1);
        if (q == std::string::npos) return {};
        auto r = line.find('"', q + 1);
        if (r == std::string::npos) return {};
        return line.substr(q + 1, r - q - 1);
    };
    auto method = findStr("method");
    std::string id = findStr("id"); // handles string ids; numeric ids will be echoed as-is below

    // params extraction (raw json object/array or omitted)
    std::string params = "{}";
    auto pp = line.find("\"params\"");
    if (pp != std::string::npos) {
        auto colon = line.find(':', pp);
        if (colon != std::string::npos) {
            // naive: take substring from colon+1 to end and trim trailing }
            params = line.substr(colon + 1);
            // strip trailing spaces/newlines
            while (!params.empty() && (params.back() == '\r' || params.back() == '\n' || params.back() == ' ')) params.pop_back();
            // if ends with } or ] followed by }, keep only inner json
            if (!params.empty() && params.front() == '{') {
                int depth = 0; size_t i = 0;
                for (; i < params.size(); ++i) {
                    if (params[i] == '{') depth++;
                    else if (params[i] == '}') { depth--; if (depth == 0) { ++i; break; } }
                }
                params = params.substr(0, i);
            } else if (!params.empty() && params.front() == '[') {
                int depth = 0; size_t i = 0;
                for (; i < params.size(); ++i) {
                    if (params[i] == '[') depth++;
                    else if (params[i] == ']') { depth--; if (depth == 0) { ++i; break; } }
                }
                params = params.substr(0, i);
            }
        }
    }

    if (method.empty()) {
        outJson = "{\"jsonrpc\":\"2.0\",\"id\":null,\"error\":{\"code\":-32600,\"message\":\"invalid request\"}}\n";
        return true;
    }

    // Dispatch
    RpcRequest req{method, id, params};
    auto res = owner_.dispatch(req.method, req.paramsJson);

    std::ostringstream ss;
    ss << "{\"jsonrpc\":\"2.0\",\"id\":";
    if (id.empty()) ss << "null";
    else ss << "\"" << id << "\"";
    ss << ",";
    if (res.ok) {
        ss << "\"result\":" << (res.json.empty() ? "null" : res.json);
    } else {
        ss << "\"error\":" << (res.json.empty() ? "{\"code\":-32000,\"message\":\"error\"}" : res.json);
    }
    ss << "}\n";
    outJson = ss.str();
    (void)fd;
    return true;
}

void RpcTcpServer::threadMain() {
    while (running_.load()) {
        fd_set rfds;
        FD_ZERO(&rfds);
        int maxfd = -1;

        if (listenFd_ >= 0) { FD_SET(listenFd_, &rfds); if (listenFd_ > maxfd) maxfd = listenFd_; }

        {
            std::lock_guard<std::mutex> lk(mu_);
            for (int fd : clients_) {
                FD_SET(fd, &rfds);
                if (fd > maxfd) maxfd = fd;
            }
        }

        timeval tv{0, 200000}; // 200ms
        int rc = select(maxfd + 1, &rfds, nullptr, nullptr, &tv);
        if (rc <= 0) continue;

        if (listenFd_ >= 0 && FD_ISSET(listenFd_, &rfds)) {
            sockaddr_in cli{}; socklen_t clen = sizeof(cli);
            int cfd = ::accept(listenFd_, reinterpret_cast<sockaddr*>(&cli), &clen);
            if (cfd >= 0) {
                set_nonblock(cfd);
                std::lock_guard<std::mutex> lk(mu_);
                clients_.push_back(cfd);
            }
        }

        std::lock_guard<std::mutex> lk(mu_);
        for (size_t i = 0; i < clients_.size();) {
            int fd = clients_[i];
            if (!FD_ISSET(fd, &rfds)) { ++i; continue; }

            char buf[2048];
            ssize_t n = ::recv(fd, buf, sizeof(buf), 0);
            if (n <= 0) {
                ::close(fd);
                clients_.erase(clients_.begin() + i);
                continue;
            }
            std::string in(buf, buf + n);
            size_t pos = 0;
            while (true) {
                auto nl = in.find('\n', pos);
                if (nl == std::string::npos) break;
                std::string line = in.substr(pos, nl - pos);
                pos = nl + 1;
                std::string out;
                if (handleLine(fd, line, out)) {
                    ::send(fd, out.data(), out.size(), 0);
                }
            }
            ++i;
        }
    }
}

} // namespace lfc
