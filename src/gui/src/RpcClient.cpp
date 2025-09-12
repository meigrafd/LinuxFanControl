#include "RpcClient.h"

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include <QJsonDocument>

RpcClient::RpcClient(const QString& sockPath)
: sock_(sockPath) {}

static int connect_unix(const QString& path) {
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

bool RpcClient::writeAll(int fd, const QByteArray& data) {
    const char* p = data.constData();
    qsizetype left = data.size();
    while (left > 0) {
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

bool RpcClient::readLine(int fd, QByteArray& line) {
    line.clear();
    char ch;
    while (true) {
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

QJsonObject RpcClient::call(const QString& method, const QJsonObject& params, const QString& id) {
    int fd = connect_unix(sock_);
    if (fd < 0) return QJsonObject{{"error", QJsonObject{{"code",-32000},{"message","connect failed"}}}};
    QJsonObject req{{"jsonrpc","2.0"},{"method",method},{"id",id}};
    if (!params.isEmpty()) req["params"] = params;
    QByteArray out = QJsonDocument(req).toJson(QJsonDocument::Compact);
    out.append('\n');
    if (!writeAll(fd, out)) { ::close(fd); return QJsonObject{{"error", QJsonObject{{"code",-32001},{"message","write failed"}}}}; }
    QByteArray line;
    if (!readLine(fd, line)) { ::close(fd); return QJsonObject{{"error", QJsonObject{{"code",-32002},{"message","read failed"}}}}; }
    ::close(fd);
    QJsonParseError pe{};
    QJsonDocument doc = QJsonDocument::fromJson(line, &pe);
    if (pe.error != QJsonParseError::NoError || !doc.isObject()) {
        return QJsonObject{{"error", QJsonObject{{"code",-32700},{"message","parse error"}}}};
    }
    return doc.object();
}

QJsonArray RpcClient::callBatch(const QJsonArray& batch) {
    int fd = connect_unix(sock_);
    if (fd < 0) return QJsonArray{ QJsonObject{{"error", QJsonObject{{"code",-32000},{"message","connect failed"}}}} };
    QByteArray out = QJsonDocument(batch).toJson(QJsonDocument::Compact);
    out.append('\n');
    if (!writeAll(fd, out)) { ::close(fd); return QJsonArray{ QJsonObject{{"error", QJsonObject{{"code",-32001},{"message","write failed"}}}} }; }
    QByteArray line;
    if (!readLine(fd, line)) { ::close(fd); return QJsonArray{ QJsonObject{{"error", QJsonObject{{"code",-32002},{"message","read failed"}}}} }; }
    ::close(fd);
    QJsonParseError pe{};
    QJsonDocument doc = QJsonDocument::fromJson(line, &pe);
    if (pe.error != QJsonParseError::NoError || !doc.isArray()) {
        return QJsonArray{ QJsonObject{{"error", QJsonObject{{"code",-32700},{"message","parse error"}}}} };
    }
    return doc.array();
}

QJsonObject RpcClient::ping()     { return call("ping"); }
QJsonObject RpcClient::version()  { return call("version"); }
QJsonObject RpcClient::enumerate(){ return call("enumerate"); }

QJsonArray RpcClient::listChannels() {
    QJsonObject r = call("listChannels");
    if (r.contains("result") && r["result"].isArray()) return r["result"].toArray();
    return QJsonArray{};
}
