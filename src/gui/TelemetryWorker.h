#pragma once
#include <QObject>
#include <QJsonArray>
#include <atomic>

class TelemetryWorker : public QObject {
    Q_OBJECT
public:
    explicit TelemetryWorker(QObject* parent=nullptr);
    ~TelemetryWorker() override = default;
public slots:
    void start();
    void stop();
signals:
    void tickReady(QJsonArray channels);
    void workerError(QString message);
private:
    std::atomic<bool> running_{false};
};
