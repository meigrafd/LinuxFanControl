#pragma once
#include <QMainWindow>
#include <QVector>
#include <QPointF>

class QComboBox;
class QLabel;
class QWidget;
class QScrollArea;
class FlowLayout;

struct UiTemp {
    QString name, path, type;
};
struct UiPwm {
    QString label, pwm, enable, tach;
};

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent=nullptr);

private:
    // UI
    QComboBox* langCombo_ = nullptr;
    QLabel* status_ = nullptr;

    QWidget* sensWrap_ = nullptr;
    QWidget* pwmWrap_  = nullptr;
    FlowLayout* sensFlow_ = nullptr;
    FlowLayout* pwmFlow_  = nullptr;

    QString theme_ = "dark";

    // Data
    QVector<UiTemp> temps_;
    QVector<UiPwm>  pwms_;

    // Build
    void buildUi();
    void applyTheme(const QString& theme);
    void rebuildCards();

    // Actions
    void onDetect();
    void onToggleTheme();

    // Data fetch
    void fetchTemps();
    void fetchPwms();
};
