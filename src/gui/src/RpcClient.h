#pragma once
// Simple newline-delimited JSON-RPC 2.0 client over Unix domain socket.
// Adds overloads with (timeoutMs, error*) to match existing call sites.
// Comments in English per project.

#include <QString>
#include <QJsonObject>
#include <QJsonArray>
#include <memory>

class RpcClient {
public:
    explicit RpcClient(const QString& sockPath = qEnvironmentVariable("LFC_SOCK", "/tmp/lfcd.sock"));

    // Existing simple calls (no explicit timeout/error)
    QJsonObject call(const QString& method,
                     const QJsonObject& params = QJsonObject(),
                     const QString& id = "1");
    QJsonArray  callBatch(const QJsonArray& batch);

    // Overloads matching existing sources (TelemetryWorker.cpp / DetectDialog.cpp):
    // NOTE: returns a pointer so call sites using 'res->...' continue to compile.
    std::unique_ptr<QJsonObject> call(const char* method,
                                      QJsonObject params,
                                      int timeoutMs,
                                      std::string* err = nullptr);
    std::unique_ptr<QJsonObject> call(const char* method,
                                      QJsonObject params,
                                      int timeoutMs,
                                      QString* err);

    // Convenience wrappers
    QJsonObject ping();
    QJsonObject version();
    QJsonObject enumerate();
    QJsonArray  listChannels();

private:
    QString sock_;

    // low-level helpers
    int  connectUnix(const QString& path);
    bool writeAll(int fd, const QByteArray& data, int timeoutMs);
    bool readLine(int fd, QByteArray& line, int timeoutMs);
};
