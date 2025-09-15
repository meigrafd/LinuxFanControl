/*
 * Minimal JSON-RPC 2.0 server over plain TCP.
 * Framing: newline-delimited JSON per request/response; batch allowed.
 * (c) 2025 LinuxFanControl contributors
 */
#include "RpcTcpServer.hpp"
#include "Daemon.hpp"
#include "JsonLite.hpp"

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <sstream>
#include <cctype>
#include <thread>

RpcTcpServer::RpcTcpServer(Daemon& d, const std::string& host, uint16_t port, bool debug)
: daemon_(d), host_(host), port_(port), debug_(debug) {}

RpcTcpServer::~RpcTcpServer() { stop(); }

bool RpcTcpServer::start(std::string& err) {
    listenfd_ = ::socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd_ < 0) { err = "socket() failed"; return false; }
    int one = 1;
    ::setsockopt(listenfd_, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
    ::setsockopt(listenfd_, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));

    sockaddr_in addr{}; addr.sin_family=AF_INET; addr.sin_port=htons(port_);
    if (::inet_pton(AF_INET, host_.c_str(), &addr.sin_addr) != 1) { err="inet_pton failed"; ::close(listenfd_); return false; }
    if (::bind(listenfd_, (sockaddr*)&addr, sizeof(addr)) != 0) { err="bind() failed"; ::close(listenfd_); return false; }
    if (::listen(listenfd_, 16) != 0) { err="listen() failed"; ::close(listenfd_); return false; }

    running_.store(true);
    thr_ = std::thread([this]{ runAcceptLoop(); });
    return true;
}

void RpcTcpServer::stop() {
    if (running_.exchange(false)) {
        if (listenfd_ >= 0) { ::shutdown(listenfd_, SHUT_RDWR); ::close(listenfd_); listenfd_=-1; }
        if (thr_.joinable()) thr_.join();
    }
}

void RpcTcpServer::runAcceptLoop() {
    while (running_.load()) {
        int cfd = ::accept(listenfd_, nullptr, nullptr);
        if (cfd < 0) { if (!running_.load()) break; continue; }
        handleClient(cfd);
        ::close(cfd);
    }
}

bool RpcTcpServer::readLine(int fd, std::string& out) {
    out.clear();
    char ch;
    while (true) {
        ssize_t n = ::recv(fd, &ch, 1, 0);
        if (n <= 0) return false;
        if (ch == '\n') break;
        out.push_back(ch);
        if (out.size() > (1<<20)) return false; // 1MB guard
    }
    // trim CR
    while (!out.empty() && (out.back()=='\r' || std::isspace((unsigned char)out.back()))) out.pop_back();
    return true;
}

void RpcTcpServer::handleClient(int fd) {
    std::string line;
    while (readLine(fd, line)) {
        bool hasAny=false;
        std::string out = handleJsonRpc(line, hasAny);
        if (hasAny) {
            out.push_back('\n');
            ::send(fd, out.data(), out.size(), 0);
        }
    }
}

std::string RpcTcpServer::handleJsonRpc(const std::string& json, bool& hasAnyResponse) {
    using namespace jsonlite;
    Value root; std::string err;
    if (!parse(json, root, err)) {
        hasAnyResponse = true;
        return "{\"jsonrpc\":\"2.0\",\"id\":null,\"error\":{\"code\":-32700,\"message\":\"Parse error\"}}";
        }

        auto one = [this](const Value& obj)->std::optional<std::string>{
            if (!obj.isObject()) {
                return std::string("{\"jsonrpc\":\"2.0\",\"id\":null,\"error\":{\"code\":-32600,\"message\":\"Invalid Request\"}}");
                }
                const Value* vmethod = objGet(obj,"method");
                const Value* vid     = objGet(obj,"id");
                const Value* vparams = objGet(obj,"params");
                if (!vmethod || !vmethod->isStr()) {
                    return std::string("{\"jsonrpc\":\"2.0\",\"id\":null,\"error\":{\"code\":-32600,\"message\":\"Invalid Request (method)\"}}");
                    }
                    bool notif = (vid==nullptr);
                    std::string idJson = notif ? "null" :
                    ( vid->isStr() ? ("\""+vid->asStr()+"\"") :
                    vid->isNum() ? stringify(*vid) :
                    vid->isBool()? (vid->asBool() ? "true":"false") : "null" );
                    std::string paramsJson = "null"; if (vparams) paramsJson = stringify(*vparams);
                    std::string inner = daemon_.dispatch(vmethod->asStr(), paramsJson);
                    if (notif) return std::nullopt;
                    // wrap into top-level if inner contains only "result" or "error" field(s)
                    std::string fields = inner;
                    if (!inner.empty() && inner.front()=='{' && inner.back()=='}') fields = inner.substr(1, inner.size()-2);
                    std::ostringstream o;
                    o << "{\"jsonrpc\":\"2.0\",\"id\":" << idJson;
                    if (!fields.empty()) o << "," << fields;
                    o << "}";
                    return o.str();
                    };

                    if (root.isArray()) {
                        const auto& arr = root.asArray();
                        std::vector<std::string> res; res.reserve(arr.size());
                        for (auto& it : arr) { auto r = one(it); if (r.has_value()) res.push_back(std::move(r.value())); }
                        if (res.empty()) { hasAnyResponse=false; return {}; }
                        std::ostringstream o; o<<"["; for (size_t i=0;i<res.size();++i){ if(i)o<<","; o<<res[i]; } o<<"]";
                        hasAnyResponse=true; return o.str();
                    }
                    auto r = one(root);
                    if (!r.has_value()) { hasAnyResponse=false; return {}; }
                    hasAnyResponse=true; return *r;
                    }
