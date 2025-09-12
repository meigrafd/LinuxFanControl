#include "TelemetryWorker.h"
#include "RpcClient.h"
#include <QThread>
#include <QJsonValue>
#include <QJsonObject>

TelemetryWorker::TelemetryWorker(QObject* parent) : QObject(parent) {}

void TelemetryWorker::start() {
    running_ = true;
    while (running_) {
        RpcClient cli;
        std::string err;
        auto res = cli.call("listChannels", QJsonObject{}, 8000, &err);
        if (!res) {
            emit workerError(QString::fromStdString(err));
            QThread::msleep(500);
            continue;
        }
        emit tickReady(res->toArray());
        QThread::msleep(1000);
    }
}

void TelemetryWorker::stop() { running_ = false; }
