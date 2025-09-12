#pragma once
// MainWindow — dashboard-style UI with tiles/cards.
// Comments in English (per project), UI strings currently English.
// Provides: Detect, Start/Stop engine, Dark/Light switch, Sensors hide/apply.

#include <QMainWindow>
#include <QMap>
#include <QSet>
#include <QJsonArray>
#include <QJsonObject>

class QSplitter;
class QScrollArea;
class QWidget;
class QLabel;
class QComboBox;
class QGridLayout;
class QPushButton;

class RpcClient;
class TelemetryWorker;

struct ChannelCardRefs {
    QWidget* card{nullptr};
    QLabel*  name{nullptr};
    QLabel*  sensor{nullptr};
    QLabel*  duty{nullptr};
    QLabel*  temp{nullptr};
};

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent=nullptr);
    ~MainWindow();

private slots:
    void refresh();              // refresh tiles from enumerate()
    void detect();               // open detect dialog, create channels, start engine
    void startEngine();
    void stopEngine();
    void switchTheme();
    void applyHideSensors();
    void onTelemetry(const QJsonArray& channels);  // periodic updates

private:
    void buildUi();
    void clearGrid(QGridLayout* g);

    QWidget* makeSensorCard(const QJsonObject& sensor, bool checked);
    QWidget* makeChannelCard(const QJsonObject& ch);

    void rebuildSensors(const QJsonArray& sensors);
    void rebuildChannels(const QJsonArray& channels);

    // basic heuristic: choose a sensor path for a given pwm label
    QString chooseSensorForPwm(const QString& pwmLabel,
                               const QJsonArray& sensors) const;

private:
    RpcClient*       rpc_{};
    TelemetryWorker* tw_{};

    QSplitter*   splitter_{};
    QScrollArea* saTop_{};
    QScrollArea* saBottom_{};
    QWidget*     wrapTop_{};
    QWidget*     wrapBottom_{};
    QGridLayout* gridTop_{};
    QGridLayout* gridBottom_{};

    QAction*     actDetect_{};
    QAction*     actRefresh_{};
    QAction*     actStart_{};
    QAction*     actStop_{};
    QAction*     actTheme_{};

    // sensor hiding
    QPushButton* btnApplyHide_{};
    QSet<QString> hiddenSensors_;         // by sensor "label"
    QMap<QString, QWidget*> sensorCards_; // label -> card widget

    // channel tiles
    QMap<QString, ChannelCardRefs> chCards_;  // id -> refs

    // cache
    QJsonArray sensorsCache_;
    QJsonArray pwmsCache_;
    bool isDark_{false};
};
