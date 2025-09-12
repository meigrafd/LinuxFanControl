#pragma once
#include <string>
#include <optional>
#include <functional>
#include <vector>
#include <QString>
#include <QByteArray>
#include <QJsonObject>
#include <QJsonValue>

/* Simple JSON-RPC 2.0 over UNIX domain socket (newline-delimited JSON).
 * Blocking calls with timeout. Suitable for GUI thread for short ops;
 * heavy ops should go into a worker thread.
 */
class RpcClient {
public:
    explicit RpcClient(QString sockPath = "/tmp/lfcd.sock");
    ~RpcClient();

    bool connect(std::string* err = nullptr);
    void close();

    // Call method with params (QJsonObject or convertible), returns result object.
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
