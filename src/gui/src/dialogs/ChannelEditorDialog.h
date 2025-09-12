#pragma once
#include <QDialog>
#include <QVector>
#include <QPointF>
class QLineEdit;
class QDoubleSpinBox;
class CurveEditor;

class ChannelEditorDialog : public QDialog {
    Q_OBJECT
public:
    struct Model {
        QString id, name;
        double hyst = 0.5;
        double tau = 2.0;
        QVector<QPointF> curve;
    };
    ChannelEditorDialog(const Model& m, QWidget* parent=nullptr);
    QString newName() const;
signals:
    void saveRequested(QString id, QString name, QVector<QPointF> curve, double hyst, double tau);
private:
    Model m_;
    QLineEdit* nameEdit_ = nullptr;
    CurveEditor* editor_ = nullptr;
    QDoubleSpinBox *spHyst_=nullptr, *spTau_=nullptr;
    void onSave();
};
