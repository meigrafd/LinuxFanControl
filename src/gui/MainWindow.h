#pragma once
#include <QMainWindow>
#include <QMap>
#include <QTimer>
#include "Translations.h"

class QSplitter;
class QScrollArea;
class QWidget;
class QComboBox;
class QLabel;
class QThread;

class FanCard;
class SensorCard;
class DetectDialog;
class ChannelEditorDialog;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override = default;

private:
    Translations tr_;
    QString theme_ = "dark";

    QSplitter* splitter_ = nullptr;
    QScrollArea *saChannels_ = nullptr, *saSensors_ = nullptr, *saPwms_ = nullptr;
    QWidget *wrapChannels_ = nullptr, *wrapSensors_ = nullptr, *wrapPwms_ = nullptr;
    QComboBox* comboLang_ = nullptr;
    QLabel* statusEngine_ = nullptr;

    struct Temp { QString label, path, type; };
    struct Pwm  { QString label, pwmPath, enablePath, tachPath; bool writable=false; };
    struct Chan {
        QString id, name, sensorPath, pwmPath, enablePath, mode;
        double manualPct=0, lastTemp=0, lastOut=0;
    };
    QList<Temp> temps_;
    QList<Pwm>  pwms_;
    QMap<QString, Chan> chans_;
    QMap<QString, bool> selSensors_;
    QMap<QString, bool> selPwms_;

    QTimer tick_;
    QThread* teleThread_ = nullptr;
    class TelemetryWorker* tele_ = nullptr;
    void startTelemetry(); void stopTelemetry();

    void buildUi();
    void applyTheme(const QString& theme);
    void retranslate();
    void rebuildSources();
    void rebuildChannelCards();
    void refreshAll();
    void refreshChannels();
    void tick();

    void onToggleTheme();
    void onSwitchLang(const QString& code);
    void onOpenDetect();
    void onCreateFromSelection();
    void onEngine(bool start);
    void onEditChannel(const QString& id);
    void onChannelContextMenu(FanCard* card, const QPoint& pos);
    void openChannelEditor(const QString& id);

    bool rpcEnumerate();
    bool rpcListChannels();
    bool rpcSetChannelMode(const QString& id, const QString& mode);
    bool rpcSetChannelManual(const QString& id, double pct);
    bool rpcDeleteChannel(const QString& id);
    bool rpcCreateChannel(const QString& name, const QString& sensor, const QString& pwm, const QString& enable);
    bool rpcSetChannelCurve(const QString& id, const QVector<QPointF>& pts);
    bool rpcSetChannelHystTau(const QString& id, double hyst, double tau);

    QString t(const char* key, const QVariantMap& args = {}) const { return tr_.t(key, args); }
};
