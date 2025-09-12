/*
 * Detect/Calibrate dialog (non-blocking, shows daemon logs).
 * (c) 2025 meigrafd & contributors - MIT
 */
#include "DetectDialog.h"
#include "../RpcClient.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QLabel>
#include <QFutureWatcher>
#include <QtConcurrent/QtConcurrent>

DetectDialog::DetectDialog(RpcClient* rpc, QWidget* parent)
: QDialog(parent), rpc_(rpc) {
    setWindowTitle(tr("Auto-Setup: Detect & Calibrate"));
    resize(820, 520);

    auto* v = new QVBoxLayout(this);
    auto* head = new QLabel(tr("Detect sensors & PWMs, infer coupling, calibrate minimum duty.\nPrevious PWM state will be restored afterwards."));
    head->setWordWrap(true);
    v->addWidget(head);

    bar_ = new QProgressBar();
    bar_->setRange(0, 0);
    v->addWidget(bar_);

    log_ = new QPlainTextEdit(); log_->setReadOnly(true);
    v->addWidget(log_);

    auto* h = new QHBoxLayout();
    btnStart_ = new QPushButton(tr("Start"));
    btnClose_ = new QPushButton(tr("Close"));
    h->addStretch(1); h->addWidget(btnStart_); h->addWidget(btnClose_);
    v->addLayout(h);

    connect(btnStart_, &QPushButton::clicked, this, &DetectDialog::onStart);
    connect(btnClose_, &QPushButton::clicked, this, &DetectDialog::onCancel);
    connect(rpc_, &RpcClient::daemonLogLine, this, &DetectDialog::onDaemonLine);
}

DetectDialog::DetectDialog(QWidget* parent)
: DetectDialog(new RpcClient(parent), parent) {}

void DetectDialog::append(const QString& s) { log_->appendPlainText(s); }

void DetectDialog::onDaemonLine(QString line) { append(line); }

void DetectDialog::onStart() {
    if (running_) return;
    running_ = true;
    btnStart_->setEnabled(false);
    btnClose_->setEnabled(false);
    append(tr("Starting…"));

    QString err;
    if (!rpc_->ensureRunning(&err)) {
        append(tr("Failed to start daemon: %1").arg(err));
        running_ = false; btnClose_->setEnabled(true); return;
    }

    // small preflight
    {
        auto s = rpc_->listSensors(&err);
        if (!err.isEmpty()) append(tr("listSensors error: %1").arg(err));
        else append(tr("Sensors discovered: %1").arg(s.size()));
        err.clear();
        auto p = rpc_->listPwms(&err);
        if (!err.isEmpty()) append(tr("listPwms error: %1").arg(err));
        else append(tr("PWMs discovered: %1").arg(p.size()));
        err.clear();
    }

    append(tr("Running detectCalibrate…"));

    auto future = QtConcurrent::run([this]() -> QPair<QJsonValue, QString> {
        return rpc_->call(QStringLiteral("detectCalibrate"), QJsonObject{}, 120000);
    });

    auto* watcher = new QFutureWatcher<QPair<QJsonValue, QString>>(this);
    connect(watcher, &QFutureWatcherBase::finished, this, [this, watcher]() {
        const auto res = watcher->result();
        watcher->deleteLater();

        bar_->setRange(0, 1); bar_->setValue(1);
        running_ = false; btnClose_->setEnabled(true);

        if (!res.second.isEmpty()) { append(tr("detectCalibrate failed: %1").arg(res.second)); return; }
        if (!res.first.isObject()) { append(tr("Unexpected payload.")); return; }
        result_ = res.first.toObject();

        const auto mapping = result_.value("mapping").toArray();
        const auto skipped = result_.value("skipped").toArray();
        const auto errors  = result_.value("errors").toArray();
        const auto calib   = result_.value("calibration").toArray();

        append(tr("Completed. Mappings: %1  |  Skipped: %2  |  Calibrated: %3  |  Errors: %4")
        .arg(mapping.size()).arg(skipped.size()).arg(calib.size()).arg(errors.size()));
        for (const auto& v : mapping) {
            const auto o = v.toObject();
            append(QString("  %1 -> %2 (score %.3f)")
            .arg(o.value("pwm").toString(),
                 o.value("sensor_label").toString())
            .arg(o.value("score").toDouble()));
        }
        if (!errors.isEmpty()) append(tr("Errors:"));
        for (const auto& v : errors) {
            const auto o = v.toObject();
            append(QString("  %1 [%2]: %3")
            .arg(o.value("device").toString(),
                 o.value("stage").toString(),
                 o.value("error").toString()));
        }
    });
    watcher->setFuture(future);
}

void DetectDialog::onCancel() {
    if (running_) { append(tr("Setup running; waiting to finish…")); return; }
    accept();
}
