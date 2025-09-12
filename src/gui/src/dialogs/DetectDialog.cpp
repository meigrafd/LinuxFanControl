/*
 * Linux Fan Control (LFC) - Detect/Calibrate Dialog
 * (c) 2025 meigrafd & contributors - MIT License (see LICENSE)
 */

#include "DetectDialog.h"
#include "../RpcClient.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QLabel>
#include <QtConcurrent>

DetectDialog::DetectDialog(RpcClient* rpc, QWidget* parent)
: QDialog(parent), rpc_(rpc) {
    setWindowTitle(tr("Auto-Setup: Detect & Calibrate"));
    resize(820, 520);

    auto* v = new QVBoxLayout(this);
    auto* head = new QLabel(tr("This will detect sensors and PWM outputs, infer coupling, and quickly calibrate minimum duty. Your previous PWM state will be restored afterwards."));
    head->setWordWrap(true);
    v->addWidget(head);

    bar_ = new QProgressBar();
    bar_->setRange(0, 0); // busy until we can compute steps
    v->addWidget(bar_);

    log_ = new QPlainTextEdit();
    log_->setReadOnly(true);
    v->addWidget(log_);

    auto* h = new QHBoxLayout();
    btnStart_ = new QPushButton(tr("Start"));
    btnClose_ = new QPushButton(tr("Close"));
    h->addStretch(1);
    h->addWidget(btnStart_);
    h->addWidget(btnClose_);
    v->addLayout(h);

    connect(btnStart_, &QPushButton::clicked, this, &DetectDialog::onStart);
    connect(btnClose_, &QPushButton::clicked, this, &DetectDialog::onCancel);
    connect(rpc_, &RpcClient::daemonLogLine, this, &DetectDialog::onDaemonLine);
}

void DetectDialog::append(const QString& s) {
    log_->appendPlainText(s);
}

void DetectDialog::onDaemonLine(QString line) {
    // forward daemon log lines into dialog; useful during setup
    append(line);
}

void DetectDialog::onStart() {
    if (running_) return;
    running_ = true;
    btnStart_->setEnabled(false);
    btnClose_->setEnabled(false);
    append(tr("Starting…"));

    // Resolve daemon and test basic RPC quickly first
    QString err;
    if (!rpc_->ensureRunning(&err)) {
        append(tr("Failed to start daemon: %1").arg(err));
        running_ = false;
        btnClose_->setEnabled(true);
        return;
    }

    // show quick counts (listSensors/listPwms) before long call
    {
        auto s = rpc_->call(QStringLiteral("listSensors"));
        if (!s.second.isEmpty()) {
            append(tr("listSensors error: %1").arg(s.second));
        } else {
            const int n = s.first.toArray().size();
            append(tr("Sensors discovered: %1").arg(n));
        }
        auto p = rpc_->call(QStringLiteral("listPwms"));
        if (!p.second.isEmpty()) {
            append(tr("listPwms error: %1").arg(p.second));
        } else {
            const int n = p.first.toArray().size();
            append(tr("PWMs discovered: %1").arg(n));
        }
    }

    // Now kick long-running detectCalibrate; we keep the bar busy mode.
    append(tr("Running detectCalibrate… (do not close)"));

    auto future = QtConcurrent::run([this]() -> QPair<QJsonValue, QString> {
        return rpc_->call(QStringLiteral("detectCalibrate"), QJsonObject{}, 120000 /*ms*/);
    });

    // Watcher to update UI when done
    auto* watcher = new QFutureWatcher<QPair<QJsonValue, QString>>(this);
    connect(watcher, &QFutureWatcherBase::finished, this, [this, watcher]() {
        const auto res = watcher->result();
        watcher->deleteLater();
        bar_->setRange(0, 1);
        bar_->setValue(1);
        running_ = false;
        btnClose_->setEnabled(true);

        if (!res.second.isEmpty()) {
            append(tr("detectCalibrate failed: %1").arg(res.second));
            return;
        }
        if (!res.first.isObject()) {
            append(tr("detectCalibrate returned unexpected payload."));
            return;
        }
        result_ = res.first.toObject();

        // Pretty-print some summary
        const auto mapping = result_.value("mapping").toArray();
        const auto skipped = result_.value("skipped").toArray();
        const auto errors  = result_.value("errors").toArray();
        const auto calib   = result_.value("calibration").toArray();

        append(tr("Completed."));
        append(tr("Mappings: %1  |  Skipped: %2  |  Calibrated: %3  |  Errors: %4")
        .arg(mapping.size()).arg(skipped.size()).arg(calib.size()).arg(errors.size()));

        for (const auto& v : mapping) {
            const auto o = v.toObject();
            append(QString("  %1  ->  %2  (score %.3f)")
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
    if (running_) {
        // We do not cancel the daemon-side long call (no cancel API right now),
        // but we allow the user to close the dialog after it finishes.
        append(tr("Setup is running; waiting to finish…"));
        return;
    }
    accept();
}
