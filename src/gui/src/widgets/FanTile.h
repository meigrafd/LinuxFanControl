#pragma once
// FanTile - compact channel card similar to FanControl tiles.
// Shows: title, sensor, duty, temp, Edit button. Dragging is handled by the QListWidget host.
// Comments in English (per project).

#include <QWidget>
#include <QString>

class QLabel;
class QPushButton;

class FanTile : public QWidget {
    Q_OBJECT
public:
    explicit FanTile(QWidget* parent = nullptr);

    void setTitle(const QString& t);
    void setSensor(const QString& s);
    void setDuty(double pct);
    void setTemp(double celsius);

signals:
    void editRequested(); // emitted when user clicks Edit

private:
    QLabel*  title_{nullptr};
    QLabel*  sensor_{nullptr};
    QLabel*  duty_{nullptr};
    QLabel*  temp_{nullptr};
    QPushButton* btnEdit_{nullptr};
};
