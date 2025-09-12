#include "dialogs/DetectDialog.h"
#include "RpcClient.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QHeaderView>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QCheckBox>
#include <QMetaObject>

DetectDialog::DetectDialog(RpcClient* rpc, QWidget* parent)
: QDialog(parent), rpc_(rpc)
{
    buildUi();
    onRefresh();
}

void DetectDialog::buildUi() {
    setWindowTitle("Detect Devices");

    auto* v = new QVBoxLayout(this);

    tbl_ = new QTableWidget(this);
    tbl_->setColumnCount(6);
    tbl_->setHorizontalHeaderLabels(QStringList() << "Use" << "Label" << "PWM" << "Enable" << "Tach" << "Sensor Type");
    tbl_->horizontalHeader()->setStretchLastSection(true);
    tbl_->verticalHeader()->setVisible(false);
    tbl_->setSelectionMode(QAbstractItemView::NoSelection);
    v->addWidget(tbl_);

    auto* h = new QHBoxLayout();
    btnRefresh_ = new QPushButton("Refresh", this);
    btnOk_      = new QPushButton("OK", this);
    btnCancel_  = new QPushButton("Cancel", this);
    h->addWidget(btnRefresh_);
    h->addStretch(1);
    h->addWidget(btnCancel_);
    h->addWidget(btnOk_);
    v->addLayout(h);

    connect(btnRefresh_, &QPushButton::clicked, this, &DetectDialog::onRefresh);
    connect(btnCancel_,  &QPushButton::clicked, this, &DetectDialog::reject);
    connect(btnOk_,      &QPushButton::clicked, this, &DetectDialog::onAccept);
}

void DetectDialog::onRefresh() {
    if (!rpc_) return;

    // Use timeout overload; avoid capturing unique_ptr in lambda:
    auto res = rpc_->call("enumerate", QJsonObject{}, /*timeout*/ 6000, static_cast<std::string*>(nullptr));
    if (!res) return;

    // Copy QJsonObject before posting to GUI thread
    QJsonObject obj = *res;

    QMetaObject::invokeMethod(this, [this, obj]() {
        if (!obj.contains("result") || !obj["result"].isObject()) return;
        populate(obj["result"].toObject());
    }, Qt::QueuedConnection);
}

void DetectDialog::populate(const QJsonObject& enumerateResult) {
    sensors_ = enumerateResult.value("sensors").toArray();
    pwms_    = enumerateResult.value("pwms").toArray();

    tbl_->setRowCount(0);
    for (const auto& v : pwms_) {
        const auto p = v.toObject();
        const QString label  = p.value("label").toString();
        const QString pwm    = p.value("pwm").toString();
        const QString enable = p.value("enable").toString();
        const QString tach   = p.value("tach").toString();

        const int r = tbl_->rowCount();
        tbl_->insertRow(r);

        auto* chk = new QCheckBox(this);
        chk->setChecked(true);
        tbl_->setCellWidget(r, 0, chk);

        tbl_->setItem(r, 1, new QTableWidgetItem(label));
        tbl_->setItem(r, 2, new QTableWidgetItem(pwm));
        tbl_->setItem(r, 3, new QTableWidgetItem(enable));
        tbl_->setItem(r, 4, new QTableWidgetItem(tach));
        // Simple sensor type guess (match by device folder name prefix)
        QString sensType = "Unknown";
        if (label.contains("amdgpu", Qt::CaseInsensitive)) sensType = "GPU";
        else if (label.contains("k10temp", Qt::CaseInsensitive) || label.contains("coretemp", Qt::CaseInsensitive)) sensType = "CPU";
        tbl_->setItem(r, 5, new QTableWidgetItem(sensType));
    }

    tbl_->resizeColumnsToContents();
}

QJsonArray DetectDialog::selectedPwms() const {
    QJsonArray out;
    for (int r = 0; r < tbl_->rowCount(); ++r) {
        auto* w = tbl_->cellWidget(r, 0);
        auto* chk = qobject_cast<QCheckBox*>(w);
        if (chk && chk->isChecked()) {
            QJsonObject o;
            o["label"]  = tbl_->item(r, 1)->text();
            o["pwm"]    = tbl_->item(r, 2)->text();
            o["enable"] = tbl_->item(r, 3)->text();
            o["tach"]   = tbl_->item(r, 4)->text();
            out.push_back(o);
        }
    }
    return out;
}

QJsonArray DetectDialog::sensors() const {
    return sensors_;
}

void DetectDialog::onAccept() {
    accept();
}
