#pragma once
#include <QFrame>
#include <QString>

class QLabel;
class QComboBox;
class QSlider;

class FanCard : public QFrame {
    Q_OBJECT
public:
    struct Model {
        QString id, name, sensorPath, pwmPath, enablePath;
        QString mode; // "Auto" or "Manual"
        double manualPct = 0.0;
        double lastTemp = 0.0;
        double lastOut  = 0.0;
    };
    explicit FanCard(const Model& m, QWidget* parent = nullptr);

    void updateTelemetry(double tempC, double outPct);
    void rename(const QString& newName);
    QString id() const { return m_.id; }

signals:
    void editRequested(QString channelId);
    void modeChanged(QString channelId, QString mode);
    void manualChanged(QString channelId, double pct);

private:
    Model m_;
    QLabel *title_, *lblTemp_, *lblOut_, *lblSensor_, *lblPwm_;
    QComboBox* mode_;
    QSlider* slider_;
    void applyCardStyle();
};
