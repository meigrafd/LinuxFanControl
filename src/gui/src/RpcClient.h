#pragma once
/*
 * Linux Fan Control (LFC) - GUI RpcClient
 * Spawns/owns the daemon process (lfcd) and speaks JSON-RPC 2.0 over stdio.
 * (c) 2025 meigrafd & contributors - MIT License (see LICENSE)
 */

#include <QObject>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include <QProcess>
#include <QMutex>
#include <QWaitCondition>
#include <QMap>

class RpcClient : public QObject {
    Q_OBJECT
public:
    explicit RpcClient(QObject* parent = nullptr);
    ~RpcClient() override;

    // Start daemon if not running yet. Returns true on success.
    bool ensureRunning(QString* err = nullptr);

    // Synchronous JSON-RPC call. Returns (result, errorString). Timeout in ms.
    QPair<QJsonValue, QString> call(const QString& method,
                                    const QJsonObject& params = {},
                                    int timeoutMs = 60000);

    // JSON-RPC 2.0 batch call. Each item should be an object with "jsonrpc","id","method","params".
    // Returns full parsed array or error string on failure.
    QPair<QJsonArray, QString> callBatch(const QJsonArray& batch,
                                         int timeoutMs = 60000);

    bool isRunning() const;

signals:
    void daemonCrashed(int exitCode, QProcess::ExitStatus status);
    void daemonLogLine(QString line);

private slots:
    void onReadyRead();
    void onErrorOccurred(QProcess::ProcessError);
    void onFinished(int exitCode, QProcess::ExitStatus status);

private:
    // Internal: send raw JSON line; returns false on immediate I/O error.
    bool sendJsonLine(const QJsonObject& obj);
    bool sendJsonLineRaw(const QJsonDocument& doc);

    // Internal: blocking wait for response id/batch until timeout.
    bool waitForId(const QString& id, QJsonValue* out, QString* err, int timeoutMs);
    bool waitForBatchIds(const QSet<QString>& ids, QJsonArray* out, QString* err, int timeoutMs);

    // Launch command built from env: LFC_DAEMON_WRAPPER + LFC_DAEMON + LFC_DAEMON_ARGS
    bool launchDaemon(QString* err);

    QString nextId();

private:
    QProcess* proc_{nullptr};
    QByteArray buf_;
    mutable QMutex mtx_;                 // protects maps & waiters
    QWaitCondition cond_;

    // pending single responses: id -> value or error envelope
    QMap<QString, QJsonValue> pending_;
    // pending batch: collect raw envelopes by id
    QMap<QString, QJsonObject> pendingEnv_;

    quint64 seq_{1};
    bool debug_{false};
};
