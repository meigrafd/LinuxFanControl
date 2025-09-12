#include "DetectDialog.h"
#include "RpcClient.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QThread>
#include <QJsonValue>

DetectDialog::DetectDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle(tr("Auto-Setup: Detect Coupling"));
    resize(720, 420);
    auto* v = new QVBoxLayout(this);
    log_ = new QPlainTextEdit(); log_->setReadOnly(true);
    bar_ = new QProgressBar(); bar_->setRange(0,0);
    v->addWidget(log_, 1);
    v->addWidget(bar_);
    auto* h = new QHBoxLayout();
    btnRun_ = new QPushButton(tr("Run Detection"));
    btnClose_= new QPushButton(tr("Close"));
    h->addStretch(1); h->addWidget(btnRun_); h->addWidget(btnClose_);
    v->addLayout(h);

    connect(btnRun_, &QPushButton::clicked, this, &DetectDialog::runDetect);
    connect(btnClose_, &QPushButton::clicked, this, &DetectDialog::reject);
}

void DetectDialog::append(const QString& s) { log_->appendPlainText(s); }

void DetectDialog::runDetect() {
    btnRun_->setEnabled(false);
    append("Starting detection …");
    auto* thr = QThread::create([this]{
        RpcClient cli; std::string err;
        auto res = cli.call("detectCoupling",
                            QJsonObject{{"hold_s",10.0},{"min_delta_c",1.0},{"rpm_delta_threshold",80}},
                            120000, &err);
        if (!res) {
            QMetaObject::invokeMethod(this, [this, err]{
                onDone(QJsonObject{}, QString::fromStdString(err));
            }, Qt::QueuedConnection);
            return;
        }
        QMetaObject::invokeMethod(this, [this, res]{
            onDone(res->toObject(), {});
        }, Qt::QueuedConnection);
    });
    connect(thr, &QThread::finished, thr, &QObject::deleteLater);
    thr->start();
}

void DetectDialog::onDone(const QJsonObject& r, const QString& err) {
    btnRun_->setEnabled(true);
    bar_->setRange(0,1); bar_->setValue(1);
    if (!err.isEmpty()) {
        append("Error: " + err);
        return;
    }
    result_ = r;
    append(QString("Detection finished. Found %1 mapping(s).").arg(r.size()));
}
