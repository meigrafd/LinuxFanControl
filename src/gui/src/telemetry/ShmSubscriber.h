#pragma once
// ShmSubscriber - reads telemetry frames from shared memory in a worker thread.
// Emits tickReady(QJsonArray of channels with id/last_out/last_temp).

#include <QObject>
#include <QJsonArray>
#include <QMap>
#include <QString>
#include <QThread>
#include <atomic>

class ShmSubscriber : public QObject {
    Q_OBJECT
public:
    explicit ShmSubscriber(QObject* parent=nullptr);
    ~ShmSubscriber();

    void start(const QString& shmName = "/lfc_telemetry", int periodMs = 200);
    void stop();

signals:
    void tickReady(const QJsonArray& channels);

private:
    void run();

    std::atomic<bool> running_{false};
    QThread* thread_{nullptr};
    QString shmName_;
    int periodMs_{200};
};
