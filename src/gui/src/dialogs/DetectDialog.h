#pragma once
// DetectDialog - runs daemon-side detectCalibrate in a worker thread,
// shows progress + log like the Python AutoSetup dialog.
// Comments in English per project guideline.

#include <QDialog>
#include <QJsonObject>
#include <QJsonArray>

class RpcClient;
class QPlainTextEdit;
class QProgressBar;
class QLabel;
class QPushButton;
class QThread;

class DetectWorker;

class DetectDialog : public QDialog {
    Q_OBJECT
public:
    explicit DetectDialog(RpcClient* rpc, QWidget* parent = nullptr);
    ~DetectDialog();

    // Returns non-empty on success (keys: sensors, pwms, mapping, cal_res)
    QJsonObject result() const { return result_; }

private slots:
    void onStart();
    void onCancel();
    void onWorkerLog(const QString& line);
    void onWorkerFinished(const QJsonObject& res);
    void onWorkerFailed(const QString& err);

private:
    void buildUi();

private:
    RpcClient* rpc_{nullptr};

    QLabel*        lblStatus_{nullptr};
    QProgressBar*  bar_{nullptr};
    QPlainTextEdit* log_{nullptr};
    QPushButton*   btnStart_{nullptr};
    QPushButton*   btnCancel_{nullptr};

    QThread*       thread_{nullptr};
    DetectWorker*  worker_{nullptr};

    QJsonObject    result_;
    bool           running_{false};
};
