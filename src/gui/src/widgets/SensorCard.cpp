#include "SensorCard.h"
#include <QVBoxLayout>
#include <QCheckBox>

// Match SensorCard.h: struct Model { QString label; ... };
// signal: void toggled(QString label, bool on);

SensorCard::SensorCard(const Model& m, bool checked, QWidget* parent)
: QWidget(parent), model_(m)
{
    auto* v = new QVBoxLayout(this);
    chk_ = new QCheckBox(model_.label, this);
    chk_->setChecked(checked);
    v->addWidget(chk_);

    #if QT_VERSION >= QT_VERSION_CHECK(6, 4, 0)
    connect(chk_, &QCheckBox::checkStateChanged, this, [this](Qt::CheckState){
        emit toggled(model_.label, chk_->isChecked());
    });
    #else
    connect(chk_, &QCheckBox::stateChanged, this, [this](int){
        emit toggled(model_.label, chk_->isChecked());
    });
    #endif
}
