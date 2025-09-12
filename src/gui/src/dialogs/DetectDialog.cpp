/*
 * Linux Fan Control (LFC)
 * (c) 2025 meigrafd & contributors - MIT License
 */

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
#include <QObject>

// Worker lives entirely in its own thread and uses its own RpcClient instance.
class DetectWorker : public QObject {
    Q_OBJECT
public:
    explicit DetectWorker(QObject* parent=nullptr) : QObject(parent) {}

public slots:
    void run() {
        emit log("Detecting sensors & PWM outputs…");
        RpcClient rpc; // separate socket on worker thread
        auto res = rpc.call("detectCalibrate");  // blocking, but we're in worker thread
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
};

// ------------------ DetectDialog ------------------

DetectDialog::DetectDialog(QWidget* parent)
: QDialog(parent) {
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
    btnStart_  = new QPushButton("Start", this);
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
    worker_ = new DetectWorker();
    worker_->moveToThread(thread_);

    connect(thread_, &QThread::started, worker_, [this]() {
        static_cast<DetectWorker*>(worker_)->run();
    });
    connect(worker_, SIGNAL(log(QString)), this, SLOT(onWorkerLog(QString)));
    connect(worker_, SIGNAL(finished(QJsonObject)), this, SLOT(onWorkerFinished(QJsonObject)));
    connect(worker_, SIGNAL(failed(QString)), this, SLOT(onWorkerFailed(QString)));
    connect(worker_, &QObject::destroyed, thread_, &QThread::quit);
    connect(thread_, &QThread::finished, thread_, &QThread::deleteLater);

    thread_->start();
    onWorkerLog("Starting detection… this may take a few minutes.");
}

void DetectDialog::onCancel() {
    if (!running_) {
        reject();
        return;
    }
    // Current daemon RPC is blocking; we cannot cancel mid-flight safely.
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

    if (worker_) { worker_->deleteLater(); worker_ = nullptr; }
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

    if (worker_) { worker_->deleteLater(); worker_ = nullptr; }
    reject();
}

// IMPORTANT for AUTOMOC when Q_OBJECT is only in this .cpp:
#include "DetectDialog.moc"
