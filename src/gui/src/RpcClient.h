#pragma once
// Simple newline-delimited JSON-RPC 2.0 client over Unix domain socket.
// Comments in English per project.

#include <QString>
#include <QJsonObject>
#include <QJsonArray>

class RpcClient {
public:
    explicit RpcClient(const QString& sockPath = qEnvironmentVariable("LFC_SOCK", "/tmp/lfcd.sock"));

    // Generic call
    QJsonObject call(const QString& method, const QJsonObject& params = QJsonObject(), const QString& id = "1");
    // Batch call
    QJsonArray  callBatch(const QJsonArray& batch);

    // Convenience
    QJsonObject ping();
    QJsonObject version();
    QJsonObject enumerate();
    QJsonArray  listChannels();

private:
    QString sock_;
    bool    readLine(int fd, QByteArray& line);
    bool    writeAll(int fd, const QByteArray& data);
};
