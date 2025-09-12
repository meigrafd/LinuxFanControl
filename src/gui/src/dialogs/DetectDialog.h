#pragma once
/*
 * Detect/Calibrate dialog (non-blocking, shows daemon logs).
 * (c) 2025 meigrafd & contributors - MIT
 */
#include <QDialog>
#include <QJsonObject>
class QPlainTextEdit; class QProgressBar; class QPushButton;
class RpcClient;

class DetectDialog : public QDialog {
    Q_OBJECT
public:
    explicit DetectDialog(RpcClient* rpc, QWidget* parent = nullptr);
    // Overload to keep MainWindow.cpp (DetectDialog dlg(this);) compiling:
    explicit DetectDialog(QWidget* parent);

    QJsonObject result() const { return result_; }

private slots:
    void onStart();
    void onCancel();
    void onDaemonLine(QString line);

private:
    void append(const QString& s);

private:
    RpcClient* rpc_;
    QPlainTextEdit* log_;
    QProgressBar* bar_;
    QPushButton* btnStart_;
    QPushButton* btnClose_;
    QJsonObject result_;
    bool running_ = false;
};
