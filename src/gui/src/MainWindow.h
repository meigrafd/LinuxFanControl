#pragma once
// MainWindow — dashboard-style UI modernized to mirror FanControl.Release:
// - TOP: draggable tiles (channels) in a QListWidget (IconMode)
// - SENSORS: collapsible panel with checkboxes + Apply Hide (not shown at top anymore)
// - BOTTOM: "Curves / Triggers / Mix" tiles (placeholders for future editing)
// - Auto-Setup if no channels exist: enumerate -> batch createChannel -> engineStart
// Comments in English.

#include <QMainWindow>
#include <QMap>
#include <QSet>
#include <QJsonArray>
#include <QJsonObject>

class QListWidget;
class QListWidgetItem;
class QSplitter;
class QScrollArea;
class QWidget;
class QLabel;
class QPushButton;
class QGridLayout;

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
    void refresh();
    void detect();
    void startEngine();
    void stopEngine();
    void switchTheme();
    void applyHideSensors();
    void onTelemetry(const QJsonArray& channels);

private:
    void buildUi();
    QWidget* makeCurveTile(const QString& title);
    QWidget* makeSensorTile(const QJsonObject& s, bool checked);
    QWidget* makeChannelTile(const QJsonObject& ch);

    void rebuildSensors(const QJsonArray& sensors);
    void rebuildChannels(const QJsonArray& channels);
    void rebuildCurves(const QJsonArray& channels);

    QString chooseSensorForPwm(const QString& pwmLabel, const QJsonArray& sensors) const;

private:
    RpcClient*       rpc_{};
    TelemetryWorker* tw_{};

    QSplitter*   splitter_{};

    // TOP: draggable channel tiles (fans)
    QListWidget* channelsList_{};
    QMap<QString, ChannelCardRefs> chCards_;      // channel id -> refs
    QMap<QString, QListWidgetItem*> chItems_;     // channel id -> list item

    // CENTER: collapsible sensors
    QWidget*     sensorsPanel_{};
    QWidget*     sensorsBody_{};
    QGridLayout* sensorsGrid_{};
    QPushButton* btnApplyHide_{};
    QSet<QString> hiddenSensors_;
    QMap<QString, QWidget*> sensorCards_;         // by sensor label

    // BOTTOM: curves/triggers/mix tiles
    QScrollArea* curvesArea_{};
    QWidget*     curvesWrap_{};
    QGridLayout* curvesGrid_{};

    QAction*     actDetect_{};
    QAction*     actRefresh_{};
    QAction*     actStart_{};
    QAction*     actStop_{};
    QAction*     actTheme_{};

    QJsonArray sensorsCache_;
    QJsonArray pwmsCache_;
    bool isDark_{false};
};
