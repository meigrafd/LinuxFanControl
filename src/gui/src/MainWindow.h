#pragma once
#include <QMainWindow>
#include <QPointer>
#include <QVector>
#include <QJsonObject>
#include <QJsonArray>

class QSplitter;
class QScrollArea;
class QWidget;
class QLabel;
class QComboBox;
class QGridLayout;

class RpcClient;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent=nullptr);
    ~MainWindow();

private slots:
    void refresh();
    void onSwitchLang(const QString& code);

private:
    void buildUi();
    void clearGrid(QGridLayout* g);
    QWidget* makeSensorCard(const QJsonObject& s);
    QWidget* makePwmCard(const QJsonObject& p);

private:
    RpcClient*   rpc_{};
    QLabel*      statusEngine_{};
    QSplitter*   splitter_{};
    QScrollArea* saTop_{};
    QScrollArea* saBottom_{};
    QWidget*     wrapTop_{};
    QWidget*     wrapBottom_{};
    QGridLayout* gridTop_{};
    QGridLayout* gridBottom_{};
    QComboBox*   comboLang_{};
};
