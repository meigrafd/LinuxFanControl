#pragma once
#include <QDialog>
#include <QJsonObject>
class QPlainTextEdit;
class QProgressBar;
class QPushButton;

class DetectDialog : public QDialog {
    Q_OBJECT
public:
    explicit DetectDialog(QWidget* parent=nullptr);
    QJsonObject result() const { return result_; }
private:
    QPlainTextEdit* log_ = nullptr;
    QProgressBar* bar_ = nullptr;
    QPushButton *btnRun_=nullptr, *btnClose_=nullptr;
    QJsonObject result_;
    void append(const QString& s);
    void runDetect();
    void onDone(const QJsonObject& r, const QString& err);
};
