#pragma once
/*
 * GUI side JSON-RPC client (spawns daemon) for lfc-gui.
 * (c) 2025 meigrafd & contributors - MIT
 */
#include <QObject>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include <QProcess>
#include <QMutex>
#include <QWaitCondition>
#include <QMap>
#include <QSet>

class RpcClient : public QObject {
    Q_OBJECT
public:
    explicit RpcClient(QObject* parent = nullptr);
    ~RpcClient() override;

    bool ensureRunning(QString* err = nullptr);
    bool isRunning() const;

    // Generic calls
    QPair<QJsonValue, QString> call(const QString& method,
                                    const QJsonObject& params = {},
                                    int timeoutMs = 60000);
    QPair<QJsonArray, QString> callBatch(const QJsonArray& batch,
                                         int timeoutMs = 60000);

    // Convenience wrappers used by GUI
    QJsonArray listSensors(QString* err = nullptr);
    QJsonArray listPwms(QString* err = nullptr);
    QJsonObject enumerate(QString* err = nullptr);  // {sensors, pwms}
    QJsonArray listChannels(QString* err = nullptr); // may be empty if daemon not yet supports

signals:
    void daemonCrashed(int exitCode, QProcess::ExitStatus status);
    void daemonLogLine(QString line);

private slots:
    void onReadyRead();
    void onErrorOccurred(QProcess::ProcessError);
    void onFinished(int exitCode, QProcess::ExitStatus status);

private:
    bool sendJsonLine(const QJsonObject& obj);
    bool sendJsonLineRaw(const QJsonDocument& doc);
    bool waitForId(const QString& id, QJsonValue* out, QString* err, int timeoutMs);
    bool waitForBatchIds(const QSet<QString>& ids, QJsonArray* out, QString* err, int timeoutMs);
    bool launchDaemon(QString* err);
    QString nextId();
    static QString shellQuote(const QString& s);

private:
    QProcess* proc_{nullptr};
    QByteArray buf_;
    mutable QMutex mtx_;
    QWaitCondition cond_;
    QMap<QString, QJsonValue> pending_;
    QMap<QString, QJsonObject> pendingEnv_;
    quint64 seq_{1};
    bool debug_{false};
};
