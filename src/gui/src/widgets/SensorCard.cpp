#include "SensorCard.h"
#include <QVBoxLayout>
#include <QCheckBox>
#include <QString>

// Match SensorCard.h:
//   struct Model { QString label; /* ... */ };
//   class SensorCard : public QFrame { Q_OBJECT ... signals: void toggled(QString,bool); }
//
// This implementation avoids a private 'model_' member and uses the ctor argument directly.

SensorCard::SensorCard(const Model& m, bool checked, QWidget* parent)
: QFrame(parent)
{
    auto* v = new QVBoxLayout(this);
    chk_ = new QCheckBox(m.label, this);
    chk_->setChecked(checked);
    v->addWidget(chk_);

    const QString lbl = m.label; // capture for lambdas

    #if QT_VERSION >= QT_VERSION_CHECK(6, 4, 0)
    connect(chk_, &QCheckBox::checkStateChanged, this, [this, lbl](Qt::CheckState){
        emit toggled(lbl, chk_->isChecked());
    });
    #else
    connect(chk_, &QCheckBox::stateChanged, this, [this, lbl](int){
        emit toggled(lbl, chk_->isChecked());
    });
    #endif
}
