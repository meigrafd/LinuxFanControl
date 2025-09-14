#include "RpcServer.hpp"
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

RpcServer::RpcServer(Daemon& d, const std::string& host, uint16_t port, bool debug)
: daemon_(d), host_(host), port_(port), debug_(debug) {}

RpcServer::~RpcServer() { stop(); }

bool RpcServer::start(std::string& err) {
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

void RpcServer::stop() {
    if (running_.exchange(false)) {
        if (listenfd_ >= 0) { ::shutdown(listenfd_, SHUT_RDWR); ::close(listenfd_); listenfd_=-1; }
        if (thr_.joinable()) thr_.join();
    }
}

void RpcServer::runAcceptLoop() {
    while (running_.load()) {
        int cfd = ::accept(listenfd_, nullptr, nullptr);
        if (cfd < 0) { if (!running_.load()) break; continue; }
        handleClient(cfd);
        ::close(cfd);
    }
}

void RpcServer::handleClient(int fd) {
    std::string method, path, body; int contentLen=0;
    if (!readHttp(fd, method, path, contentLen, body)) {
        auto r = makeHttpResponse(400, "{\"error\":\"bad request\"}");
        ::send(fd, r.data(), r.size(), 0); return;
    }
    if (method!="POST" || path!="/rpc") {
        auto r = makeHttpResponse(404, "{\"error\":\"not found\"}");
        ::send(fd, r.data(), r.size(), 0); return;
    }
    bool hasAny=false;
    std::string out = handleJsonRpc(body, hasAny);
    if (!hasAny) {
        std::string resp = "HTTP/1.1 204 No Content\r\nConnection: close\r\n\r\n";
        ::send(fd, resp.data(), resp.size(), 0); return;
    }
    auto resp = makeHttpResponse(200, out);
    ::send(fd, resp.data(), resp.size(), 0);
}

bool RpcServer::readHttp(int fd, std::string& method, std::string& path, int& contentLen, std::string& body) {
    std::string hdr; char buf[4096];
    while (true) {
        ssize_t n = ::recv(fd, buf, sizeof(buf), 0);
        if (n <= 0) return false;
        hdr.append(buf, buf+n);
        auto pos = hdr.find("\r\n\r\n");
        if (pos != std::string::npos) {
            std::string header = hdr.substr(0, pos+4);
            auto eol = header.find("\r\n"); if (eol==std::string::npos) return false;
            { std::istringstream is(header.substr(0,eol)); is >> method >> path; }
            contentLen = 0;
            { std::istringstream hs(header); std::string line;
                while (std::getline(hs, line)) {
                    if (!line.empty() && line.back()=='\r') line.pop_back();
                    if (line.rfind("Content-Length:",0)==0) contentLen = std::stoi(line.substr(15));
                }
            }
            body = hdr.substr(pos+4);
            break;
        }
        if (hdr.size() > (1<<20)) return false;
    }
    while ((int)body.size() < contentLen) {
        ssize_t n = ::recv(fd, buf, sizeof(buf), 0);
        if (n <= 0) return false;
        body.append(buf, buf+n);
    }
    return true;
}

static std::string trim(const std::string& s) {
    size_t a=0,b=s.size(); while (a<b && std::isspace((unsigned char)s[a])) ++a; while (b>a && std::isspace((unsigned char)s[b-1])) --b; return s.substr(a,b-a);
}

std::string RpcServer::buildEnvelope(const std::string& idJson, const std::string& inner) {
    std::string in = trim(inner);
    std::string fields = in;
    if (!in.empty() && in.front()=='{' && in.back()=='}') fields = in.substr(1, in.size()-2);
    std::ostringstream o;
    o << "{\"jsonrpc\":\"2.0\",\"id\":" << idJson;
    if (!fields.empty()) o << "," << fields;
    o << "}";
    return o.str();
}

std::string RpcServer::handleJsonRpc(const std::string& json, bool& hasAnyResponse) {
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
                    return buildEnvelope(idJson, inner);
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

                    std::string RpcServer::makeHttpResponse(int code, const std::string& body, const char* ctype) {
                        std::ostringstream o; const char* text="OK";
                        if (code==204) text="No Content"; else if (code==400) text="Bad Request"; else if (code==404) text="Not Found"; else if (code>=500) text="Server Error";
                        o << "HTTP/1.1 " << code << " " << text << "\r\n";
                        if (code!=204) { o << "Content-Type: " << ctype << "\r\n"; o << "Content-Length: " << body.size() << "\r\n"; }
                        o << "Connection: close\r\n\r\n";
                        if (code!=204) o << body;
                        return o.str();
                    }
