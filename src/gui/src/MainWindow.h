#pragma once
// MainWindow — dashboard-like UI mirroring FanControl.Release:
// - TOP: draggable compact tiles for channels
// - Sensors panel collapsed by default (only visible after Setup/Refresh)
// - Bottom Curves area hidden until channels exist
// - No auto-detection: show empty-state hint and a Setup button
// Comments in English.

#include <QMainWindow>
#include <QMap>
#include <QSet>
#include <QJsonArray>
#include <QJsonObject>

class QListWidget;
class QListWidgetItem;
class QWidget;
class QGridLayout;
class QPushButton;
class QAction;

class RpcClient;
class TelemetryWorker;
class FanTile;

struct ChannelCardRefs {
    QWidget* card{nullptr};
};

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent=nullptr);
    ~MainWindow();

private slots:
    void refresh();
    void detect();          // opens DetectDialog (no freeze)
    void startEngine();
    void stopEngine();
    void switchTheme();
    void applyHideSensors();
    void onTelemetry(const QJsonArray& channels);

private:
    void buildUi();
    void showEmptyState(bool on);
    QWidget* makeFanTileWidget(const QJsonObject& ch);
    QString chooseSensorForPwm(const QString& pwmLabel, const QJsonArray& sensors) const;

    void rebuildSensors(const QJsonArray& sensors);
    void rebuildChannels(const QJsonArray& channels);

private:
    RpcClient*       rpc_{};
    TelemetryWorker* tw_{};

    // TOP: draggable channel tiles (fans)
    QListWidget* channelsList_{};
    QMap<QString, ChannelCardRefs> chCards_;
    QMap<QString, QListWidgetItem*> chItems_;

    // Sensors panel (collapsed by default)
    QWidget*     sensorsPanel_{};
    QGridLayout* sensorsGrid_{};
    QPushButton* btnApplyHide_{};
    QSet<QString> hiddenSensors_;
    QMap<QString, QWidget*> sensorCards_;

    // Empty-state overlay
    QWidget*     emptyState_{};
    QPushButton* btnEmptySetup_{};

    QAction*     actSetup_{};
    QAction*     actRefresh_{};
    QAction*     actStart_{};
    QAction*     actStop_{};
    QAction*     actTheme_{};

    QJsonArray sensorsCache_;
    QJsonArray pwmsCache_;
    bool isDark_{true};
};
