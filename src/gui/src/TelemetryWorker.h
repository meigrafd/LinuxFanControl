#pragma once
// Periodic pull of telemetry via JSON-RPC (listChannels).
// Comments in English per project guideline.

#include <QObject>
#include <QJsonArray>

class QTimer;
class RpcClient;

class TelemetryWorker : public QObject {
    Q_OBJECT
public:
    explicit TelemetryWorker(RpcClient* rpc, QObject* parent = nullptr);

public slots:
    void start(int intervalMs = 1000);
    void stop();

signals:
    // Emits the current channels array from daemon (listChannels result)
    void tickReady(const QJsonArray& channels);

private slots:
    void pollOnce();

private:
    RpcClient* rpc_{nullptr};
    QTimer*    timer_{nullptr};
};
