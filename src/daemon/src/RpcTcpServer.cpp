/*
 * Linux Fan Control — RPC TCP Server (implementation)
 * - Minimal JSON-RPC 2.0 over TCP (newline-delimited)
 * - Non-blocking accept + client handling
 * (c) 2025 LinuxFanControl contributors
 */
#include "RpcTcpServer.hpp"
#include "Daemon.hpp"
#include "include/CommandRegistry.h"
#include "Log.hpp"

#include <nlohmann/json.hpp>

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <string>
#include <vector>
#include <unordered_map>

using nlohmann::json;

namespace lfc {

static int set_nonblock(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

RpcTcpServer::RpcTcpServer(Daemon& owner, const std::string& host, unsigned short port, bool verbose)
    : owner_(owner), host_(host), port_(port), verbose_(verbose) {}

RpcTcpServer::~RpcTcpServer() {
    stop();
}

bool RpcTcpServer::start(CommandRegistry* reg) {
    reg_ = reg;
    if (running_) return true;

    listenFd_ = ::socket(AF_INET, SOCK_STREAM, 0);
    if (listenFd_ < 0) return false;

    int yes = 1;
    setsockopt(listenFd_, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(port_);
    addr.sin_addr.s_addr = inet_addr(host_.c_str());
    if (bind(listenFd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        ::close(listenFd_);
        listenFd_ = -1;
        return false;
    }
    if (listen(listenFd_, 8) < 0) {
        ::close(listenFd_);
        listenFd_ = -1;
        return false;
    }
    if (set_nonblock(listenFd_) != 0) {
        ::close(listenFd_);
        listenFd_ = -1;
        return false;
    }

    running_.store(true);
    thr_ = std::thread([this]{ loop(); });
    return true;
}

void RpcTcpServer::stop() {
    running_.store(false);
    if (listenFd_ >= 0) {
        ::shutdown(listenFd_, SHUT_RDWR);
        ::close(listenFd_);
        listenFd_ = -1;
    }
    if (thr_.joinable()) thr_.join();

    for (auto& kv : clients_) {
        ::shutdown(kv.first, SHUT_RDWR);
        ::close(kv.first);
    }
    clients_.clear();
}

void RpcTcpServer::loop() {
    while (running_.load()) {
        fd_set rfds; FD_ZERO(&rfds);
        int maxfd = -1;

        if (listenFd_ >= 0) {
            FD_SET(listenFd_, &rfds);
            maxfd = std::max(maxfd, listenFd_);
        }

        for (const auto& kv : clients_) {
            FD_SET(kv.first, &rfds);
            maxfd = std::max(maxfd, kv.first);
        }

        timeval tv{0, 200000}; // 200ms
        int rc = select(maxfd + 1, &rfds, nullptr, nullptr, &tv);
        if (rc < 0) {
            if (errno == EINTR) continue;
            break;
        }

        // Accept
        if (listenFd_ >= 0 && FD_ISSET(listenFd_, &rfds)) {
            int cfd = accept(listenFd_, nullptr, nullptr);
            if (cfd >= 0) {
                set_nonblock(cfd);
                clients_.emplace(cfd, Client{});
            }
        }

        // Read
        std::vector<int> toClose;
        for (auto& kv : clients_) {
            int fd = kv.first;
            if (!FD_ISSET(fd, &rfds)) continue;

            char buf[4096];
            ssize_t n = ::recv(fd, buf, sizeof(buf), 0);
            if (n <= 0) { toClose.push_back(fd); continue; }

            kv.second.acc.append(buf, static_cast<size_t>(n));
            size_t pos;
            while ((pos = kv.second.acc.find('\n')) != std::string::npos) {
                std::string line = kv.second.acc.substr(0, pos);
                kv.second.acc.erase(0, pos + 1);

                std::string reply;
                if (handleLine(fd, line, reply)) {
                    reply.push_back('\n');
                    (void)::send(fd, reply.data(), reply.size(), MSG_NOSIGNAL);
                }
            }
        }
        for (int fd : toClose) {
            ::shutdown(fd, SHUT_RDWR);
            ::close(fd);
            clients_.erase(fd);
        }
    }
}

bool RpcTcpServer::handleLine(int /*fd*/, const std::string& line, std::string& outReply) {
    std::string method, id, paramsJson;

    // Parse JSON-RPC or plain "method"
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
        // fallback to plain
    }

    if (method.empty()) {
        method = line;
        paramsJson = "{}";
    }

    // DISPATCH via CommandRegistry (Daemon has no dispatch() member)
    RpcResult res;
    if (reg_) {
        RpcRequest req{method, id, paramsJson};
        res = reg_->call(req);
    } else {
        res = RpcResult{false, "{\"error\":\"no registry\"}"};
    }

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
            outReply = wrapper.dump();
            return true;
        } catch (...) {
            // fallback: send raw
        }
    }

    outReply = res.json;
    return true;
}

} // namespace lfc
