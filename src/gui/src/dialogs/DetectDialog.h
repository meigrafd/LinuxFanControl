#pragma once
// DetectDialog - lists discovered PWMs + sensors; user confirms selection.

#include <QDialog>
#include <QJsonArray>
#include <QJsonObject>

class RpcClient;
class QTableWidget;
class QPushButton;

class DetectDialog : public QDialog {
    Q_OBJECT
public:
    explicit DetectDialog(RpcClient* rpc, QWidget* parent = nullptr);

    void populate(const QJsonObject& enumerateResult);
    QJsonArray selectedPwms() const;
    QJsonArray sensors() const;

private slots:
    void onRefresh();
    void onAccept();

private:
    void buildUi();

private:
    RpcClient* rpc_{nullptr};
    QTableWidget* tbl_{nullptr};
    QPushButton* btnRefresh_{nullptr};
    QPushButton* btnOk_{nullptr};
    QPushButton* btnCancel_{nullptr};

    QJsonArray sensors_;
    QJsonArray pwms_;
};
