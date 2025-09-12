#include "telemetry/ShmSubscriber.h"
#include "common/ShmTelemetry.h"

#include <QJsonObject>
#include <QDateTime>

#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

using namespace lfc::shm;

ShmSubscriber::ShmSubscriber(QObject* parent) : QObject(parent) {}
ShmSubscriber::~ShmSubscriber() { stop(); }

void ShmSubscriber::start(const QString& shmName, int periodMs) {
    stop();
    shmName_ = shmName;
    periodMs_ = periodMs;
    running_.store(true);
    thread_ = QThread::create([this]() { this->run(); });
    thread_->start();
}

void ShmSubscriber::stop() {
    running_.store(false);
    if (thread_) { thread_->quit(); thread_->wait(1000); delete thread_; thread_ = nullptr; }
}

void ShmSubscriber::run() {
    // Map existing SHM (created by daemon)
    Mapping m{};
    if (!createOrOpen(m, shmName_.toUtf8().constData(), /*capacity ignored*/1024, /*create*/false)) {
        // silently fail; no telemetry available
        return;
    }
    uint32_t last = m.hdr->write_idx.load(std::memory_order_acquire);

    while (running_.load()) {
        const uint32_t cur = m.hdr->write_idx.load(std::memory_order_acquire);
        if (cur != last) {
            // collect latest per id
            QMap<QString, QJsonObject> latest;
            uint32_t start = last;
            uint32_t end   = cur;
            const uint32_t cap = m.hdr->capacity;
            for (uint32_t i = start; i < end; ++i) {
                const TelemetryFrame& f = m.frames[cap ? (i % cap) : 0];
                QString id = QString::fromUtf8(f.id);
                QJsonObject o;
                o["id"] = id;
                o["last_out"]  = f.duty;
                o["last_temp"] = f.temp;
                latest[id] = o;
            }
            last = cur;

            // pack to array
            QJsonArray arr;
            for (auto it = latest.begin(); it != latest.end(); ++it) arr.push_back(it.value());
            if (!arr.isEmpty()) emit tickReady(arr);
        }
        QThread::msleep(periodMs_);
    }
    destroy(m);
}
