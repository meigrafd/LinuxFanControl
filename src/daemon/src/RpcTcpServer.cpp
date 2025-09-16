/*
 * Linux Fan Control — RPC TCP Server (implementation)
 * - Minimal JSON-RPC 2.0 over TCP (newline-delimited)
 * - Non-blocking accept + client handling via select()
 * (c) 2025 LinuxFanControl contributors
 */
#include "RpcTcpServer.hpp"
#include "include/CommandRegistry.h"
#include "Daemon.hpp"
#include "Log.hpp"

#include <nlohmann/json.hpp>
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstring>
#include <vector>

using nlohmann::json;

namespace lfc {

static int set_nonblock(int fd) {
    int fl = fcntl(fd, F_GETFL, 0);
    if (fl < 0) return -1;
    if (fcntl(fd, F_SETFL, fl | O_NONBLOCK) < 0) return -1;
    return 0;
}

RpcTcpServer::RpcTcpServer(Daemon& owner, const std::string& host, unsigned short port, bool verbose)
    : owner_(owner), host_(host), port_(port), verbose_(verbose) {}

RpcTcpServer::~RpcTcpServer() {
    stop();
}

bool RpcTcpServer::start(CommandRegistry* reg) {
    reg_ = reg;
    if (!reg_) return false;

    listenFd_ = ::socket(AF_INET, SOCK_STREAM, 0);
    if (listenFd_ < 0) {
        LFC_LOGE("rpc: socket() failed: %s", std::strerror(errno));
        return false;
    }

    int on = 1;
    setsockopt(listenFd_, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port_);
    addr.sin_addr.s_addr = host_.empty() ? htonl(INADDR_LOOPBACK) : inet_addr(host_.c_str());
    if (addr.sin_addr.s_addr == INADDR_NONE) {
        LFC_LOGE("rpc: invalid bind host '%s'", host_.c_str());
        ::close(listenFd_);
        listenFd_ = -1;
        return false;
    }

    if (::bind(listenFd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        LFC_LOGE("rpc: bind(%s:%u) failed: %s", host_.c_str(), (unsigned)port_, std::strerror(errno));
        ::close(listenFd_);
        listenFd_ = -1;
        return false;
    }

    if (::listen(listenFd_, 16) < 0) {
        LFC_LOGE("rpc: listen() failed: %s", std::strerror(errno));
        ::close(listenFd_);
        listenFd_ = -1;
        return false;
    }

    if (set_nonblock(listenFd_) < 0) {
        LFC_LOGW("rpc: failed to set non-blocking listen socket");
    }

    running_.store(true);
    thr_ = std::thread([this]{ loop(); });
    return true;
}

void RpcTcpServer::stop() {
    if (!running_.exchange(false)) return;

    if (listenFd_ >= 0) {
        ::shutdown(listenFd_, SHUT_RDWR);
        ::close(listenFd_);
        listenFd_ = -1;
    }

    if (thr_.joinable()) thr_.join();

    for (auto& kv : clients_) {
        ::close(kv.first);
    }
    clients_.clear();
}

bool RpcTcpServer::handleLine(int fd, const std::string& line, std::string& outJson) {
    (void)fd; // explicitly unused; kept for potential per-client logging later

    if (!reg_) { outJson = R"({"error":"rpc not ready"})"; return true; }

    std::string method;
    std::string id;
    std::string paramsJson = "{}";

    // Try JSON-RPC 2.0, fall back to "plain" (method only) lines
    try {
        if (!line.empty() && (line[0] == '{' || line[0] == '[')) {
            json j = json::parse(line);

            if (j.is_object()) {
                method = j.value("method", "");
                if (j.contains("id")) {
                    if (j["id"].is_string()) id = j["id"].get<std::string>();
                    else if (j["id"].is_number_integer()) id = std::to_string(j["id"].get<long long>());
                }
                if (j.contains("params")) {
                    if (j["params"].is_object() || j["params"].is_array()) {
                        paramsJson = j["params"].dump();
                    } else if (j["params"].is_string()) {
                        paramsJson = j["params"].get<std::string>();
                    }
                }
            }
        }
    } catch (...) {
        // ignore, fallback below
    }

    if (method.empty()) {
        method = line;
        paramsJson = "{}";
    }

    RpcRequest rq;
    rq.method = method;
    rq.id     = id;
    rq.params = paramsJson;

    auto res = reg_->call(rq);

    if (!id.empty()) {
        try {
            json payload = json::parse(res.json);
            json wrapper;
            wrapper["jsonrpc"] = "2.0";
            wrapper["id"] = id;
            if (res.ok) wrapper["result"] = payload;
            else {
                wrapper["error"] = payload.is_object() ? payload : json{{"message", payload.dump()}};
            }
            outJson = wrapper.dump();
            return true;
        } catch (...) {
            // fall through: raw
        }
    }

    outJson = res.json;
    return true;
}

void RpcTcpServer::loop() {
    while (running_.load()) {
        fd_set rfds;
        FD_ZERO(&rfds);
        int maxfd = -1;

        if (listenFd_ >= 0) {
            FD_SET(listenFd_, &rfds);
            maxfd = std::max(maxfd, listenFd_);
        }

        for (auto& kv : clients_) {
            FD_SET(kv.first, &rfds);
            maxfd = std::max(maxfd, kv.first);
        }

        timeval tv{};
        tv.tv_sec = 0;
        tv.tv_usec = 200000; // 200 ms tick

        int rc = ::select(maxfd + 1, &rfds, nullptr, nullptr, &tv);
        if (rc < 0) {
            if (errno == EINTR) continue;
            if (running_.load()) LFC_LOGW("rpc: select() error: %s", std::strerror(errno));
            continue;
        }

        if (listenFd_ >= 0 && FD_ISSET(listenFd_, &rfds)) {
            sockaddr_in cli{};
            socklen_t clilen = sizeof(cli);
            int cfd = ::accept(listenFd_, reinterpret_cast<sockaddr*>(&cli), &clilen);
            if (cfd >= 0) {
                set_nonblock(cfd);
                clients_.emplace(cfd, Client{});
            }
        }

        std::vector<int> toClose;

        for (auto& kv : clients_) {
            int cfd = kv.first;
            auto& cl = kv.second;
            if (!FD_ISSET(cfd, &rfds)) continue;

            char buf[1024];
            ssize_t n = ::recv(cfd, buf, sizeof(buf), 0);
            if (n <= 0) {
                toClose.push_back(cfd);
                continue;
            }
            cl.acc.append(buf, buf + n);

            for (;;) {
                auto pos = cl.acc.find('\n');
                if (pos == std::string::npos) break;
                std::string line = cl.acc.substr(0, pos);
                cl.acc.erase(0, pos + 1);

                if (line.empty()) continue;

                std::string reply;
                if (handleLine(cfd, line, reply)) {
                    reply.push_back('\n');
                    (void)::send(cfd, reply.data(), reply.size(), MSG_NOSIGNAL);
                }
            }
        }

        for (int cfd : toClose) {
            ::close(cfd);
            clients_.erase(cfd);
        }
    }
}

} // namespace lfc
