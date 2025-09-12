/*
 * Linux Fan Control (LFC) - Main Window
 * (c) 2025 meigrafd & contributors - MIT License
 */
#pragma once
#include <QMainWindow>
#include <QMap>
#include <QSet>
#include <QJsonArray>
#include <QJsonObject>

class QListWidget;
class QListWidgetItem;
class QGridLayout;
class QWidget;
class QAction;
class QPushButton;
class QLabel;

class RpcClient;
class ShmSubscriber;
class FanTile;

struct ChannelCardRefs {
    FanTile* card{nullptr};
};

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent=nullptr);
    ~MainWindow();

private slots:
    void switchTheme();
    void startEngine();
    void stopEngine();
    void detect();
    void onImport();
    void refresh();
    void onTelemetry(const QJsonArray& channels);
    void applyHideSensors();

private:
    void showEmptyState(bool on);
    QWidget* makeFanTileWidget(const QJsonObject& ch);
    void rebuildChannels(const QJsonArray& channels);
    void rebuildSensors(const QJsonArray& sensors);
    QString chooseSensorForPwm(const QJsonArray& sensors, const QString& pwmLabel) const;

    // Inline editor dialog (defined in .cpp)
    void openEditorForChannel(const QString& channelId, const QJsonObject& channelObj);

private:
    RpcClient* rpc_{nullptr};
    ShmSubscriber* shm_{nullptr};
    bool isDark_{true};

    // toolbar actions
    QAction* actSetup_{};
    QAction* actImport_{};
    QAction* actRefresh_{};
    QAction* actStart_{};
    QAction* actStop_{};
    QAction* actTheme_{};

    // Empty state
    QWidget*  emptyState_{};
    QPushButton* btnEmptySetup_{};

    // Top tiles
    QListWidget* channelsList_{};
    QMap<QString, QListWidgetItem*> chItems_;
    QMap<QString, ChannelCardRefs>  chCards_;

    // Sensors panel (hidden by default, user can hide entries then "Apply Hide")
    QWidget*   sensorsPanel_{};
    QGridLayout* sensorsGrid_{};
    QPushButton* btnApplyHide_{};
    QSet<QString> hiddenSensors_;
    QJsonArray sensorsCache_;
};
