/*
 * Linux Fan Control — RPC TCP Server (implementation)
 * - Plain TCP accept loop, per-connection handler
 * - Line-based framing with naive JSON routing
 * (c) 2025 LinuxFanControl contributors
 */
#include "RpcTcpServer.hpp"
#include "Daemon.hpp"

#include <thread>
#include <atomic>
#include <string>
#include <vector>
#include <optional>
#include <cstring>

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>

namespace lfc {

RpcTcpServer::RpcTcpServer(Daemon& d, const std::string& host, std::uint16_t port, bool debug)
: daemon_(d), host_(host), port_(port), debug_(debug) {}

RpcTcpServer::~RpcTcpServer() {
    stop();
}

bool RpcTcpServer::start(CommandRegistry* reg) {
    reg_ = reg;

    listenfd_ = ::socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd_ < 0) return false;

    int one = 1;
    ::setsockopt(listenfd_, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port_);
    if (::inet_pton(AF_INET, host_.c_str(), &addr.sin_addr) != 1) {
        ::close(listenfd_); listenfd_ = -1; return false;
    }
    if (::bind(listenfd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        ::close(listenfd_); listenfd_ = -1; return false;
    }
    if (::listen(listenfd_, 16) != 0) {
        ::close(listenfd_); listenfd_ = -1; return false;
    }

    running_.store(true);
    thr_ = std::thread([this] { runAcceptLoop(); });
    return true;
}

void RpcTcpServer::stop() {
    if (running_.exchange(false)) {
        if (listenfd_ >= 0) {
            ::shutdown(listenfd_, SHUT_RDWR);
            ::close(listenfd_);
            listenfd_ = -1;
        }
        if (thr_.joinable()) thr_.join();
    }
}

void RpcTcpServer::pumpOnce(int /*timeoutMs*/) {
    // no-op; server uses its own thread
}

void RpcTcpServer::runAcceptLoop() {
    while (running_.load()) {
        int cfd = ::accept(listenfd_, nullptr, nullptr);
        if (cfd < 0) {
            if (!running_.load()) break;
            continue;
        }
        std::thread(&RpcTcpServer::handleClient, this, cfd).detach();
    }
}

bool RpcTcpServer::readLine(int fd, std::string& out) {
    out.clear();
    char buf[512];
    while (true) {
        ssize_t n = ::recv(fd, buf, sizeof(buf), 0);
        if (n <= 0) return false;
        for (ssize_t i = 0; i < n; ++i) {
            char ch = buf[i];
            if (ch == '\n') return true;
            out.push_back(ch);
        }
        if (out.size() > 1<<20) return false;
    }
}

void RpcTcpServer::handleClient(int fd) {
    std::string line;
    while (readLine(fd, line)) {
        bool hasAny = false;
        std::string out = handleJsonRpc(line, hasAny);
        if (hasAny) {
            out.push_back('\n');
            ::send(fd, out.data(), out.size(), MSG_NOSIGNAL);
        }
    }
    ::shutdown(fd, SHUT_RDWR);
    ::close(fd);
}

static std::optional<std::string> extractField(const std::string& json, const char* key) {
    std::string pat = std::string("\"") + key + "\":";
    auto p = json.find(pat);
    if (p == std::string::npos) return std::nullopt;
    p += pat.size();
    while (p < json.size() && (json[p] == ' ' || json[p] == '\"')) {
        if (json[p] == '\"') { // string value
            std::size_t q = json.find('\"', p + 1);
            if (q == std::string::npos) return std::nullopt;
            return json.substr(p + 1, q - (p + 1));
        }
        ++p;
    }
    // fallback: assume raw JSON value
    std::size_t q = json.find_first_of(",}", p);
    if (q == std::string::npos) return std::nullopt;
    return json.substr(p, q - p);
}

std::string RpcTcpServer::handleJsonRpc(const std::string& json, bool& hasAnyResponse) {
    hasAnyResponse = false;
    auto m = extractField(json, "method");
    if (!m) return "{\"error\":{\"code\":-32600,\"message\":\"invalid request\"}}";
    auto p = extractField(json, "params");
    std::string params = p.value_or("null");
    std::string res = daemon_.dispatch(*m, params);
    hasAnyResponse = true;
    return res;
}

std::vector<std::string> RpcTcpServer::listMethods() const {
    std::vector<std::string> v;
    if (!reg_) return v;
    auto list = reg_->list();
    v.reserve(list.size());
    for (auto& ci : list) v.push_back(ci.name);
    return v;
}

} // namespace lfc
