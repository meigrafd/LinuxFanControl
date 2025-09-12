#include "dialogs/DetectDialog.h"
#include "RpcClient.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QLabel>
#include <QPushButton>
#include <QThread>
#include <QTimer>

class DetectWorker : public QObject {
    Q_OBJECT
public:
    explicit DetectWorker(RpcClient* rpc, QObject* parent=nullptr) : QObject(parent), rpc_(rpc) {}

public slots:
    void run() {
        emit log("Detecting sensors & PWM outputs…");
        // Single blocking RPC — we run it in this thread to avoid UI freeze.
        auto res = rpc_->call("detectCalibrate");
        if (!res.contains("result")) {
            emit failed("detectCalibrate failed");
            return;
        }
        emit log("Detection + calibration finished.");
        emit finished(res.value("result").toObject());
    }

signals:
    void log(const QString& line);
    void finished(const QJsonObject& res);
    void failed(const QString& err);

private:
    RpcClient* rpc_{nullptr};
};

DetectDialog::DetectDialog(RpcClient* rpc, QWidget* parent)
: QDialog(parent), rpc_(rpc) {
    setWindowTitle("Auto-Setup (Detect & Calibrate)");
    resize(720, 420);
    buildUi();
}

DetectDialog::~DetectDialog() {
    if (thread_) {
        thread_->quit();
        thread_->wait(3000);
        thread_->deleteLater();
    }
}

void DetectDialog::buildUi() {
    auto* v = new QVBoxLayout(this);

    lblStatus_ = new QLabel("Idle", this);
    bar_ = new QProgressBar(this);
    bar_->setRange(0, 0); // indeterminate
    bar_->setTextVisible(false);
    bar_->setVisible(false);

    log_ = new QPlainTextEdit(this);
    log_->setReadOnly(true);
    log_->setMinimumHeight(260);

    auto* h = new QHBoxLayout();
    h->addStretch(1);
    btnStart_ = new QPushButton("Start", this);
    btnCancel_ = new QPushButton("Close", this);
    h->addWidget(btnStart_);
    h->addWidget(btnCancel_);

    v->addWidget(lblStatus_);
    v->addWidget(bar_);
    v->addWidget(log_);
    v->addLayout(h);

    connect(btnStart_,  &QPushButton::clicked, this, &DetectDialog::onStart);
    connect(btnCancel_, &QPushButton::clicked, this, &DetectDialog::onCancel);
}

void DetectDialog::onStart() {
    if (running_) return;

    running_ = true;
    lblStatus_->setText("Running…");
    bar_->setVisible(true);
    btnStart_->setEnabled(false);
    btnCancel_->setText("Cancel");

    // Worker thread
    thread_ = new QThread(this);
    worker_ = new DetectWorker(rpc_);
    worker_->moveToThread(thread_);

    connect(thread_, &QThread::started, worker_, &DetectWorker::run);
    connect(worker_, &DetectWorker::log, this, &DetectDialog::onWorkerLog);
    connect(worker_, &DetectWorker::finished, this, &DetectDialog::onWorkerFinished);
    connect(worker_, &DetectWorker::failed, this, &DetectDialog::onWorkerFailed);
    connect(worker_, &DetectWorker::finished, worker_, &DetectWorker::deleteLater);
    connect(worker_, &DetectWorker::failed,   worker_, &DetectWorker::deleteLater);
    connect(thread_, &QThread::finished, thread_, &QThread::deleteLater);

    thread_->start();
    onWorkerLog("Starting detection… this may take a few minutes.");
}

void DetectDialog::onCancel() {
    if (!running_) {
        reject();
        return;
    }
    // The current daemon RPC is blocking; we can't cancel mid-flight.
    // Communicate this clearly and wait until it returns.
    onWorkerLog("Cancel requested: current phase will finish, then dialog closes.");
    btnCancel_->setEnabled(false);
}

void DetectDialog::onWorkerLog(const QString& line) {
    log_->appendPlainText(line);
}

void DetectDialog::onWorkerFinished(const QJsonObject& res) {
    running_ = false;
    result_ = res;
    lblStatus_->setText("Completed");
    bar_->setVisible(false);
    btnCancel_->setText("Close");
    btnCancel_->setEnabled(true);

    if (thread_) {
        thread_->quit();
        thread_->wait(3000);
        thread_->deleteLater();
        thread_ = nullptr;
    }
    accept();
}

void DetectDialog::onWorkerFailed(const QString& err) {
    running_ = false;
    lblStatus_->setText("Failed");
    bar_->setVisible(false);
    onWorkerLog(err);
    btnStart_->setEnabled(true);
    btnCancel_->setText("Close");
    btnCancel_->setEnabled(true);

    if (thread_) {
        thread_->quit();
        thread_->wait(3000);
        thread_->deleteLater();
        thread_ = nullptr;
    }
    reject();
}
