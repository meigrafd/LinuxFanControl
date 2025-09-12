/*
 * Linux Fan Control (LFC) - Detect/Calibrate Dialog
 * (c) 2025 meigrafd & contributors - MIT License (see LICENSE)
 */

#pragma once
#include <QDialog>
#include <QJsonObject>
#include <QJsonArray>

class QPlainTextEdit;
class QProgressBar;
class QPushButton;
class RpcClient;

class DetectDialog : public QDialog {
    Q_OBJECT
public:
    explicit DetectDialog(RpcClient* rpc, QWidget* parent = nullptr);

    // Last result from detectCalibrate
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
