#include "SensorCard.h"
#include <QVBoxLayout>
#include <QCheckBox>

// Match SensorCard.h: signal void toggled(QString label, bool on);
// and ctor signature SensorCard(const Model& m, bool checked, QWidget* parent)

SensorCard::SensorCard(const Model& m, bool checked, QWidget* parent)
{
    if (parent) setParent(parent);

    auto* v = new QVBoxLayout(this);
    chk_ = new QCheckBox(m.title, this);
    chk_->setChecked(checked);
    v->addWidget(chk_);

    #if QT_VERSION >= QT_VERSION_CHECK(6, 4, 0)
    connect(chk_, &QCheckBox::checkStateChanged, this, [this, lbl=m.title](Qt::CheckState){
        emit toggled(lbl, chk_->isChecked());
    });
    #else
    connect(chk_, &QCheckBox::stateChanged, this, [this, lbl=m.title](int){
        emit toggled(lbl, chk_->isChecked());
    });
    #endif
}
