#pragma once
#include <string>
#include <optional>
#include <QString>
#include <QByteArray>
#include <QJsonObject>
#include <QJsonValue>

/* Simple JSON-RPC 2.0 over UNIX domain socket (newline-delimited JSON).
 * Blocking calls with timeout. GUI offloads heavy calls to worker threads.
 */
class RpcClient {
public:
    explicit RpcClient(QString sockPath = "/tmp/lfcd.sock");
    ~RpcClient();

    bool connect(std::string* err = nullptr);
    void close();

    std::optional<QJsonValue> call(const QString& method,
                                   const QJsonObject& params,
                                   int timeoutMs,
                                   std::string* err = nullptr);

private:
    int fd_ = -1;
    QString path_;
    int nextId_ = 1;

    bool writeLine(const QByteArray& line, std::string* err);
    bool readLine(QByteArray& lineOut, int timeoutMs, std::string* err);
};
