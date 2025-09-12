#pragma once
#include <QMainWindow>
#include <QPointer>

class QSplitter;
class QScrollArea;
class QWidget;
class QLabel;
class QComboBox;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent=nullptr);
    ~MainWindow();

private slots:
    void retranslate();
    void onSwitchLang(const QString& code);
    void rebuildChannelCards();
    void rebuildSources();
    void refreshAll();
    void startTelemetry();
    void onEngine(bool start);
    void onOpenDetect();

private:
    QLabel*      statusEngine_{};
    QSplitter*   splitter_{};
    QScrollArea* saChannels_{};
    QWidget*     wrapChannels_{};
    QComboBox*   comboLang_{};
};
