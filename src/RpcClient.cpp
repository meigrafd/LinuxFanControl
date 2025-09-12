#include "RpcClient.h"
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <poll.h>
#include <cstring>
#include <QJsonDocument>

RpcClient::RpcClient(QString sockPath) : path_(std::move(sockPath)) {}
RpcClient::~RpcClient() { close(); }

bool RpcClient::connect(std::string* err) {
    if (fd_ != -1) return true;
    int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) { if (err) *err = "socket() failed"; return false; }
    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    QByteArray p = path_.toUtf8();
    if (p.size() >= (int)sizeof(addr.sun_path)) {
        if (err) *err = "socket path too long";
        ::close(fd); return false;
    }
    std::memset(addr.sun_path, 0, sizeof(addr.sun_path));
    std::memcpy(addr.sun_path, p.constData(), p.size());
    if (::connect(fd, (sockaddr*)&addr, sizeof(addr)) != 0) {
        if (err) *err = "connect() failed (is daemon running?)";
        ::close(fd); return false;
    }
    fd_ = fd;
    return true;
}

void RpcClient::close() {
    if (fd_ != -1) { ::close(fd_); fd_ = -1; }
}

bool RpcClient::writeLine(const QByteArray& line, std::string* err) {
    const char* data = line.constData();
    size_t left = (size_t)line.size();
    while (left > 0) {
        ssize_t n = ::write(fd_, data, left);
        if (n <= 0) { if (err) *err = "write() failed"; return false; }
        left -= (size_t)n; data += n;
    }
    return true;
}

bool RpcClient::readLine(QByteArray& out, int timeoutMs, std::string* err) {
    out.clear();
    while (true) {
        pollfd pfd{ fd_, POLLIN, 0 };
        int pr = ::poll(&pfd, 1, timeoutMs);
        if (pr <= 0) { if (err) *err = "timeout or poll() failed"; return false; }
        char ch;
        ssize_t n = ::read(fd_, &ch, 1);
        if (n <= 0) { if (err) *err = "read() failed"; return false; }
        if (ch == '\n') break;
        out.push_back(ch);
    }
    return true;
}

std::optional<QJsonValue> RpcClient::call(const QString& method,
                                          const QJsonObject& params,
                                          int timeoutMs,
                                          std::string* err) {
    if (!connect(err)) return std::nullopt;
    int id = nextId_++;
    QJsonObject req{{"id", id}, {"method", method}, {"params", params}};
    QByteArray line = QJsonDocument(req).toJson(QJsonDocument::Compact);
    line.push_back('\n');
    if (!writeLine(line, err)) return std::nullopt;

    QByteArray resp;
    if (!readLine(resp, timeoutMs, err)) return std::nullopt;
    auto doc = QJsonDocument::fromJson(resp);
    if (!doc.isObject()) { if (err) *err = "invalid JSON"; return std::nullopt; }
    QJsonObject obj = doc.object();
    if (obj.contains("error")) {
        if (err) *err = obj["error"].toVariant().toString().toStdString();
        return std::nullopt;
    }
    return obj.value("result");
                                          }
