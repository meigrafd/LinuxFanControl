#pragma once
// DetectDialog - runs daemon-side detectCalibrate in a worker thread,
// shows progress + log like the Python AutoSetup dialog.
// Comments in English per project guideline.

#include <QDialog>
#include <QJsonObject>

class QPlainTextEdit;
class QProgressBar;
class QLabel;
class QPushButton;
class QThread;

// We do NOT pass an RpcClient from the UI thread to avoid cross-thread socket usage.
// The worker will create its own RpcClient in its own thread.

class DetectDialog : public QDialog {
    Q_OBJECT
public:
    explicit DetectDialog(QWidget* parent = nullptr);
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
    QLabel*         lblStatus_{nullptr};
    QProgressBar*   bar_{nullptr};
    QPlainTextEdit* log_{nullptr};
    QPushButton*    btnStart_{nullptr};
    QPushButton*    btnCancel_{nullptr};

    QThread*        thread_{nullptr};
    QObject*        worker_{nullptr}; // created in cpp

    QJsonObject     result_;
    bool            running_{false};
};
