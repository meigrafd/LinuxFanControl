#pragma once
// Detection dialog: enumerates sensors + PWMs via RPC and displays a selection.
// Comments in English per project guideline.

#include <QDialog>
#include <QJsonObject>
#include <QJsonArray>

class QPushButton;
class QTableWidget;
class RpcClient;

class DetectDialog : public QDialog {
    Q_OBJECT
public:
    explicit DetectDialog(RpcClient* rpc, QWidget* parent = nullptr);

    // Access selections after Accepted
    QJsonArray selectedPwms() const;     // array of objects {label,pwm,enable,tach}
    QJsonArray sensors() const;          // last enumerated sensors

private slots:
    void onRefresh();
    void onAccept();

private:
    void buildUi();
    void populate(const QJsonObject& enumerateResult);

private:
    RpcClient*   rpc_{nullptr};
    QTableWidget* tbl_{nullptr};
    QPushButton*  btnRefresh_{nullptr};
    QPushButton*  btnOk_{nullptr};
    QPushButton*  btnCancel_{nullptr};

    QJsonArray pwms_;
    QJsonArray sensors_;
};
