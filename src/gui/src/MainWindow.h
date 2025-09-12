#pragma once
// MainWindow — dashboard-style UI with draggable tiles for channels.
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
class QListWidget;
class QListWidgetItem;

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
    void refresh();              // refresh from enumerate(); auto-detect if no channels
    void detect();               // enumerate -> createChannel (batch) -> engineStart
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

    QString chooseSensorForPwm(const QString& pwmLabel, const QJsonArray& sensors) const;

private:
    RpcClient*       rpc_{};
    TelemetryWorker* tw_{};

    QSplitter*   splitter_{};

    // TOP: draggable tiles
    QListWidget* channelsList_{};

    // BOTTOM: sensors with hide/apply
    QScrollArea* saBottom_{};
    QWidget*     wrapBottom_{};
    QGridLayout* gridBottom_{};
    QPushButton* btnApplyHide_{};

    QAction*     actDetect_{};
    QAction*     actRefresh_{};
    QAction*     actStart_{};
    QAction*     actStop_{};
    QAction*     actTheme_{};

    QSet<QString> hiddenSensors_;           // by sensor label
    QMap<QString, QWidget*> sensorCards_;   // label -> card
    QMap<QString, ChannelCardRefs> chCards_;// channel id -> refs
    QMap<QString, QListWidgetItem*> chItems_;// channel id -> list item

    QJsonArray sensorsCache_;
    QJsonArray pwmsCache_;
    bool isDark_{false};
};
