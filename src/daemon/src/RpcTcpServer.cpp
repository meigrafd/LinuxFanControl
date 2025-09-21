/*
 * Linux Fan Control — RPC TCP Server (implementation)
 * Minimal newline-delimited JSON-RPC 2.0 over TCP.
 * (c) 2025 LinuxFanControl contributors
 */
#include <algorithm>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#include <nlohmann/json.hpp>
#include <string>
#include <vector>

#include "include/RpcTcpServer.hpp"
#include "include/Daemon.hpp"
#include "include/Log.hpp"
#include "include/CommandRegistry.hpp"

namespace lfc {

using nlohmann::json;

static inline void set_nonblock_(int fd) {
    int fl = ::fcntl(fd, F_GETFL, 0);
    if (fl < 0) return;
    (void)::fcntl(fd, F_SETFL, fl | O_NONBLOCK);
}

RpcTcpServer::RpcTcpServer(Daemon& owner, const std::string& host, unsigned short port, bool verbose)
: owner_(owner), host_(host), port_(port), verbose_(verbose) {}

RpcTcpServer::~RpcTcpServer() {
    stop();
}

bool RpcTcpServer::start(CommandRegistry* reg) {
    if (running_.load()) return true;

    reg_ = reg;

    listenFd_ = ::socket(AF_INET, SOCK_STREAM, 0);
    if (listenFd_ < 0) {
        LOG_ERROR("rpc: socket() failed: %s", std::strerror(errno));
        return false;
    }

    int one = 1;
    (void)::setsockopt(listenFd_, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(port_);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK); // bind to 127.0.0.1
    (void)host_;

    if (::bind(listenFd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        LOG_ERROR("rpc: bind() failed: %s", std::strerror(errno));
        ::close(listenFd_);
        listenFd_ = -1;
        return false;
    }
    if (::listen(listenFd_, 16) < 0) {
        LOG_ERROR("rpc: listen() failed: %s", std::strerror(errno));
        ::close(listenFd_);
        listenFd_ = -1;
        return false;
    }
    set_nonblock_(listenFd_);

    running_.store(true);
    try {
        thr_ = std::thread([this]{ this->loop_(); });
    } catch (const std::exception& ex) {
        LOG_ERROR("rpc: failed to start thread: %s", ex.what());
        ::close(listenFd_);
        listenFd_ = -1;
        running_.store(false);
        return false;
    }

    LOG_INFO("rpc: listening on 127.0.0.1:%u", port_);
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

    LOG_INFO("rpc: stopped");
}

void RpcTcpServer::loop_() {
    while (running_.load()) {
        fd_set rfds;
        FD_ZERO(&rfds);

        int maxfd = -1;
        if (listenFd_ >= 0) {
            FD_SET(listenFd_, &rfds);
            maxfd = std::max(maxfd, listenFd_);
        }

        for (const auto& kv : clients_) {
            FD_SET(kv.first, &rfds);
            maxfd = std::max(maxfd, kv.first);
        }

        timeval tv{1, 0}; // 1s tick
        int r = ::select(maxfd + 1, &rfds, nullptr, nullptr, &tv);
        if (r < 0) {
            if (errno == EINTR) continue;
            LOG_WARN("rpc: select() failed: %s", std::strerror(errno));
            continue;
        }

        if (listenFd_ >= 0 && FD_ISSET(listenFd_, &rfds)) {
            sockaddr_in cli{};
            socklen_t clilen = sizeof(cli);
            int cfd = ::accept(listenFd_, reinterpret_cast<sockaddr*>(&cli), &clilen);
            if (cfd >= 0) {
                set_nonblock_(cfd);
                clients_.emplace(cfd, Client{});
                LOG_DEBUG("rpc: client connected (fd=%d)", cfd);
            }
        }

        std::vector<int> toClose;

        for (auto& kv : clients_) {
            int cfd = kv.first;
            auto& cl = kv.second;
            if (!FD_ISSET(cfd, &rfds)) continue;

            char buf[2048];
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
                if (handleLine_(cfd, line, reply)) {
                    reply.push_back('\n');
                    (void)::send(cfd, reply.data(), reply.size(), MSG_NOSIGNAL);
                }
            }
        }

        for (int cfd : toClose) {
            LOG_DEBUG("rpc: client disconnected (fd=%d)", cfd);
            ::close(cfd);
            clients_.erase(cfd);
        }
    }
}

bool RpcTcpServer::handleLine_(int /*fd*/, const std::string& line, std::string& outJson) {
    try {
        json req = json::parse(line);

        RpcRequest r;
        r.method = req.value("method", "");
        r.params = req.contains("params") ? req["params"] : json();
        r.id     = req.contains("id") ? req["id"] : json();

        const std::string idStr = r.id.is_null() ? std::string{} : r.id.dump();
        if (verbose_) {
            LOG_DEBUG("rpc: call method='%s' id='%s'", r.method.c_str(), idStr.c_str());
        }

        RpcResult res = reg_->call(r);
        outJson = res.toJson().dump();
        return true;
    } catch (const CommandNotFound&) {
        json j = json::parse(line, nullptr, false);
        json reply{
            {"jsonrpc","2.0"},
            {"id", j.contains("id") ? j["id"] : json()},
            {"error", {{"code", -32601}, {"message", "Method not found"}}}
        };
        outJson = reply.dump();
        return true;
    } catch (const std::exception& ex) {
        LOG_WARN("rpc: bad request: %s", ex.what());
        json j = json::parse(line, nullptr, false);
        json reply{
            {"jsonrpc","2.0"},
            {"id", j.contains("id") ? j["id"] : json()},
            {"error", {{"code", -32600}, {"message", "Invalid Request"}, {"data", ex.what()}}}
        };
        outJson = reply.dump();
        return true;
    }
}

} // namespace lfc
