#include "RpcClient.h"

#include <sys/socket.h>
#include <sys/un.h>
#include <sys/poll.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include <QJsonDocument>

RpcClient::RpcClient(const QString& sockPath)
: sock_(sockPath) {}

int RpcClient::connectUnix(const QString& path) {
    int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return -1;
    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    QByteArray p = path.toLocal8Bit();
    ::snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", p.constData());
    if (::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        ::close(fd);
        return -1;
    }
    return fd;
}

bool RpcClient::writeAll(int fd, const QByteArray& data, int timeoutMs) {
    const char* p = data.constData();
    qsizetype left = data.size();

    while (left > 0) {
        // wait for POLLOUT
        struct pollfd pf{};
        pf.fd = fd;
        pf.events = POLLOUT;
        int pr = ::poll(&pf, 1, timeoutMs);
        if (pr <= 0) {
            if (pr < 0 && errno == EINTR) continue;
            return false;
        }
        ssize_t w = ::write(fd, p, static_cast<size_t>(left));
        if (w < 0) {
            if (errno == EINTR) continue;
            return false;
        }
        left -= w;
        p += w;
    }
    return true;
}

bool RpcClient::readLine(int fd, QByteArray& line, int timeoutMs) {
    line.clear();
    char ch;
    while (true) {
        // wait for POLLIN
        struct pollfd pf{};
        pf.fd = fd;
        pf.events = POLLIN;
        int pr = ::poll(&pf, 1, timeoutMs);
        if (pr <= 0) {
            if (pr < 0 && errno == EINTR) continue;
            return false;
        }
        ssize_t r = ::read(fd, &ch, 1);
        if (r == 0) return false;
        if (r < 0) {
            if (errno == EINTR) continue;
            return false;
        }
        if (ch == '\n') break;
        line.append(ch);
        if (line.size() > (1<<20)) return false; // 1MB guard
    }
    return true;
}

// ----------------- Legacy/simple API (no timeout param) -----------------

QJsonObject RpcClient::call(const QString& method, const QJsonObject& params, const QString& id) {
    int fd = connectUnix(sock_);
    if (fd < 0) return QJsonObject{{"error", QJsonObject{{"code",-32000},{"message","connect failed"}}}};
    QJsonObject req{{"jsonrpc","2.0"},{"method",method},{"id",id}};
    if (!params.isEmpty()) req["params"] = params;
    QByteArray out = QJsonDocument(req).toJson(QJsonDocument::Compact);
    out.append('\n');
    if (!writeAll(fd, out, /*timeout*/8000)) { ::close(fd); return QJsonObject{{"error", QJsonObject{{"code",-32001},{"message","write failed"}}}}; }
    QByteArray line;
    if (!readLine(fd, line, /*timeout*/8000)) { ::close(fd); return QJsonObject{{"error", QJsonObject{{"code",-32002},{"message","read failed"}}}}; }
    ::close(fd);
    QJsonParseError pe{};
    QJsonDocument doc = QJsonDocument::fromJson(line, &pe);
    if (pe.error != QJsonParseError::NoError || !doc.isObject()) {
        return QJsonObject{{"error", QJsonObject{{"code",-32700},{"message","parse error"}}}};
    }
    return doc.object();
}

QJsonArray RpcClient::callBatch(const QJsonArray& batch) {
    int fd = connectUnix(sock_);
    if (fd < 0) return QJsonArray{ QJsonObject{{"error", QJsonObject{{"code",-32000},{"message","connect failed"}}}} };
    QByteArray out = QJsonDocument(batch).toJson(QJsonDocument::Compact);
    out.append('\n');
    if (!writeAll(fd, out, /*timeout*/8000)) { ::close(fd); return QJsonArray{ QJsonObject{{"error", QJsonObject{{"code",-32001},{"message","write failed"}}}} }; }
    QByteArray line;
    if (!readLine(fd, line, /*timeout*/8000)) { ::close(fd); return QJsonArray{ QJsonObject{{"error", QJsonObject{{"code",-32002},{"message","read failed"}}}} }; }
    ::close(fd);
    QJsonParseError pe{};
    QJsonDocument doc = QJsonDocument::fromJson(line, &pe);
    if (pe.error != QJsonParseError::NoError || !doc.isArray()) {
        return QJsonArray{ QJsonObject{{"error", QJsonObject{{"code",-32700},{"message","parse error"}}}} };
    }
    return doc.array();
}

// -------- Overloads with timeout and error pointer (as expected) --------

std::unique_ptr<QJsonObject> RpcClient::call(const char* method,
                                             QJsonObject params,
                                             int timeoutMs,
                                             std::string* err) {
    int fd = connectUnix(sock_);
    if (fd < 0) {
        if (err) *err = "connect failed";
        return std::make_unique<QJsonObject>(QJsonObject{{"error", QJsonObject{{"code",-32000},{"message","connect failed"}}}});
    }
    QJsonObject req{{"jsonrpc","2.0"},{"method",QString::fromUtf8(method)},{"id","1"}};
    if (!params.isEmpty()) req["params"] = params;
    QByteArray out = QJsonDocument(req).toJson(QJsonDocument::Compact);
    out.append('\n');
    if (!writeAll(fd, out, timeoutMs)) {
        if (err) *err = "write failed";
        ::close(fd);
        return std::make_unique<QJsonObject>(QJsonObject{{"error", QJsonObject{{"code",-32001},{"message","write failed"}}}});
    }
    QByteArray line;
    if (!readLine(fd, line, timeoutMs)) {
        if (err) *err = "read failed";
        ::close(fd);
        return std::make_unique<QJsonObject>(QJsonObject{{"error", QJsonObject{{"code",-32002},{"message","read failed"}}}});
    }
    ::close(fd);
    QJsonParseError pe{};
    QJsonDocument doc = QJsonDocument::fromJson(line, &pe);
    if (pe.error != QJsonParseError::NoError || !doc.isObject()) {
        if (err) *err = "parse error";
        return std::make_unique<QJsonObject>(QJsonObject{{"error", QJsonObject{{"code",-32700},{"message","parse error"}}}});
    }
    return std::make_unique<QJsonObject>(doc.object());
}

std::unique_ptr<QJsonObject> RpcClient::call(const char* method,
                                            QJsonObject params,
                                            int timeoutMs,
                                            QString* err) {
    std::string s;
    auto res = call(method, std::move(params), timeoutMs, &s);
    if (err) *err = QString::fromStdString(s);
    return res;
}

// --------------------------- convenience -------------------------------

QJsonObject RpcClient::ping()      { return call("ping"); }
QJsonObject RpcClient::version()   { return call("version"); }
QJsonObject RpcClient::enumerate() { return call("enumerate"); }

QJsonArray RpcClient::listChannels() {
    QJsonObject r = call("listChannels");
    if (r.contains("result") && r["result"].isArray()) return r["result"].toArray();
    return QJsonArray{};
}
