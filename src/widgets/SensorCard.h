#pragma once
#include <QFrame>
#include <QString>

class QCheckBox;
class QLabel;

class SensorCard : public QFrame {
    Q_OBJECT
public:
    struct Model {
        QString label, path, type;
    };
    explicit SensorCard(const Model& m, bool selectable = true, QWidget* parent = nullptr);

    QString label() const { return m_.label; }
    void setSelected(bool on);
    bool isSelected() const;

signals:
    void toggled(QString label, bool on);

private:
    Model m_;
    QCheckBox* chk_;
    QLabel* title_, *lblType_, *lblPath_;
    void applyCardStyle();
};
