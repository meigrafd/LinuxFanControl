#include "TelemetryWorker.h"
#include "RpcClient.h"

#include <QTimer>
#include <QJsonObject>
#include <QJsonArray>

TelemetryWorker::TelemetryWorker(RpcClient* rpc, QObject* parent)
: QObject(parent), rpc_(rpc), timer_(nullptr) {}

void TelemetryWorker::start(int intervalMs) {
    if (!timer_) {
        timer_ = new QTimer(this);
        connect(timer_, &QTimer::timeout, this, &TelemetryWorker::pollOnce);
    }
    if (!timer_->isActive()) {
        timer_->start(intervalMs);
    }
    // Initial tick
    pollOnce();
}

void TelemetryWorker::stop() {
    if (timer_) timer_->stop();
}

void TelemetryWorker::pollOnce() {
    if (!rpc_) return;

    // Use RpcClient overload with timeout; unwrap "result" as array.
    auto res = rpc_->call("listChannels", QJsonObject{}, /*timeoutMs*/ 4000, static_cast<std::string*>(nullptr));
    QJsonArray arr;
    if (res && res->contains("result") && (*res)["result"].isArray()) {
        arr = (*res)["result"].toArray();
    }
    emit tickReady(arr);
}
