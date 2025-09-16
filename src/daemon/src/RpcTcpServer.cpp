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
    addr.sin_port = htons(port_);
    addr.sin_addr.s_addr = inet_addr(host_.c_str());
    if (addr.sin_addr.s_addr == INADDR_NONE) {
        ::close(listenFd_);
        listenFd_ = -1;
        return false;
    }

    if (::bind(listenFd_, (sockaddr*)&addr, sizeof(addr)) != 0) {
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
    running_ = true;
    thr_ = std::thread(&RpcTcpServer::loop, this);
    return true;
}

void RpcTcpServer::stop() {
    if (!running_) return;
    running_ = false;

    if (thr_.joinable()) thr_.join();

    for (auto& kv : clients_) {
        ::close(kv.first);
    }
    clients_.clear();

    if (listenFd_ != -1) {
        ::close(listenFd_);
        listenFd_ = -1;
    }
}

void RpcTcpServer::loop() {
    while (running_) {
        fd_set rfds;
        FD_ZERO(&rfds);
        int maxfd = -1;

        if (listenFd_ != -1) {
            FD_SET(listenFd_, &rfds);
            maxfd = listenFd_;
        }

        for (auto& kv : clients_) {
            int fd = kv.first;
            FD_SET(fd, &rfds);
            if (fd > maxfd) maxfd = fd;
        }

        timeval tv{0, 200 * 1000}; // 200ms
        int rc = ::select(maxfd + 1, &rfds, nullptr, nullptr, &tv);
        if (rc < 0) {
            if (errno == EINTR) continue;
            break;
        }

        if (listenFd_ != -1 && FD_ISSET(listenFd_, &rfds)) {
            sockaddr_in cli{};
            socklen_t sl = sizeof(cli);
            int cfd = ::accept(listenFd_, (sockaddr*)&cli, &sl);
            if (cfd >= 0) {
                set_nonblock(cfd);
                clients_[cfd] = Client{};
            }
        }

        std::vector<int> toClose;
        for (auto& kv : clients_) {
            int fd = kv.first;
            if (!FD_ISSET(fd, &rfds)) continue;

            char buf[4096];
            ssize_t n = ::recv(fd, buf, sizeof(buf), 0);
            if (n <= 0) {
                if (n == 0 || (errno != EAGAIN && errno != EWOULDBLOCK)) {
                    toClose.push_back(fd);
                }
                continue;
            }

            std::string& acc = kv.second.acc;
            acc.append(buf, buf + n);

            for (;;) {
                auto p = acc.find('\n');
                if (p == std::string::npos) break;
                std::string line = acc.substr(0, p);
                acc.erase(0, p + 1);

                std::string reply;
                if (handleLine(fd, line, reply)) {
                    reply.push_back('\n');
                    ::send(fd, reply.data(), reply.size(), 0);
                }
            }
        }

        for (int fd : toClose) {
            ::close(fd);
            clients_.erase(fd);
        }
    }
}

bool RpcTcpServer::handleLine(int /*fd*/, const std::string& line, std::string& outReply) {
    std::string method;
    std::string id;
    std::string paramsJson;

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

    auto res = owner_.dispatch(method, paramsJson);

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
