#include "SensorCard.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QCheckBox>

SensorCard::SensorCard(const Model& m, bool selectable, QWidget* parent)
: QFrame(parent), m_(m) {
    setProperty("card", true);
    applyCardStyle();
    auto* v = new QVBoxLayout(this);
    auto* head = new QHBoxLayout();
    chk_ = new QCheckBox(); chk_->setChecked(false); chk_->setEnabled(selectable);
    title_ = new QLabel(m_.label); title_->setWordWrap(true); title_->setStyleSheet("font-weight:600;");
    head->addWidget(chk_); head->addWidget(title_, 1);
    v->addLayout(head);
    lblType_ = new QLabel(QString("Type: %1").arg(m_.type));
    lblPath_ = new QLabel(QString("Path: %1").arg(m_.path));
    v->addWidget(lblType_); v->addWidget(lblPath_);

    connect(chk_, &QCheckBox::stateChanged, this, [this](int){
        emit toggled(m_.label, chk_->isChecked());
    });
}

void SensorCard::applyCardStyle() {
    setStyleSheet(R"(
    QFrame[card="true"] {
      border-radius: 12px;
      border: 1px solid rgba(0,0,0,0.12);
      padding: 12px;
      background: palette(base);
    }
    )");
}

void SensorCard::setSelected(bool on) { chk_->setChecked(on); }
bool  SensorCard::isSelected() const  { return chk_->isChecked(); }
