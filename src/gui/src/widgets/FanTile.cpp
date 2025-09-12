/*
 * Linux Fan Control (LFC) - FanTile widget
 * (c) 2025 meigrafd & contributors - MIT License
 */
#include "widgets/FanTile.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <cmath>

FanTile::FanTile(QWidget* parent) : QWidget(parent) {
    setObjectName("fanTile");
    setMinimumSize(260, 100);

    auto* v = new QVBoxLayout(this);
    v->setContentsMargins(10,8,10,8);
    v->setSpacing(6);

    auto* top = new QHBoxLayout();
    lblTitle_ = new QLabel("<b>[FAN] –</b>", this);
    btnEdit_  = new QPushButton("Edit", this);
    btnEdit_->setFixedWidth(56);
    connect(btnEdit_, &QPushButton::clicked, this, [this]{ emit editClicked(); });
    top->addWidget(lblTitle_, 1);
    top->addWidget(btnEdit_, 0);
    v->addLayout(top);

    lblSensor_ = new QLabel("[TEMP] –", this);
    v->addWidget(lblSensor_);

    auto* h = new QHBoxLayout();
    barDuty_ = new QProgressBar(this);
    barDuty_->setRange(0, 100);
    barDuty_->setValue(0);
    barDuty_->setFormat("%p%");
    lblDutyText_ = new QLabel("0%", this);
    lblTemp_     = new QLabel("-- °C", this);
    h->addWidget(barDuty_, 1);
    h->addWidget(lblDutyText_);
    h->addWidget(lblTemp_);
    v->addLayout(h);
}

void FanTile::setTitle(const QString& t) { lblTitle_->setText("<b>"+t.toHtmlEscaped()+"</b>"); }
void FanTile::setSensor(const QString& s) { lblSensor_->setText(s.toHtmlEscaped()); }
void FanTile::setDuty(double pct) {
    int v = std::lround(std::max(0.0, std::min(100.0, pct)));
    barDuty_->setValue(v);
    lblDutyText_->setText(QString::number(v) + "%");
}
void FanTile::setTemp(double celsius) {
    if (std::isfinite(celsius)) lblTemp_->setText(QString::number(celsius, 'f', 1) + " °C");
    else lblTemp_->setText("-- °C");
}
