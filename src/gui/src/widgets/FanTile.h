/*
 * Linux Fan Control (LFC) - FanTile widget
 * (c) 2025 meigrafd & contributors - MIT License
 */
#pragma once
#include <QWidget>

class QLabel;
class QProgressBar;
class QPushButton;

class FanTile : public QWidget {
    Q_OBJECT
public:
    explicit FanTile(QWidget* parent=nullptr);

    void setTitle(const QString& t);
    void setSensor(const QString& s);
    void setDuty(double pct);      // 0..100
    void setTemp(double celsius);  // may be NaN

signals:
    void editClicked();

private:
    QLabel* lblTitle_{};
    QLabel* lblSensor_{};
    QLabel* lblDutyText_{};
    QLabel* lblTemp_{};
    QProgressBar* barDuty_{};
    QPushButton* btnEdit_{};
};
