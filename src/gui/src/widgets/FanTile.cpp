#include "widgets/FanTile.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>

FanTile::FanTile(QWidget* parent)
: QWidget(parent)
{
    setObjectName("fanTile");
    // Rounded card look; the QListWidget's gridSize defines outer footprint.
    setStyleSheet(
        "QWidget#fanTile {"
        "  background: palette(base);"
        "  border: 1px solid rgba(0,0,0,30);"
        "  border-radius: 14px;"
        "}"
        "QLabel { font-size: 14px; }"
        "QLabel#title { font-weight: 600; font-size: 16px; }"
    );

    auto* v = new QVBoxLayout(this);
    v->setContentsMargins(12, 10, 12, 10);
    v->setSpacing(6);

    title_  = new QLabel("Channel", this); title_->setObjectName("title");
    sensor_ = new QLabel("sensor: --", this);
    duty_   = new QLabel("duty: -- %", this);
    temp_   = new QLabel("temp: -- °C", this);

    auto* row1 = new QHBoxLayout();
    row1->addWidget(title_, 1);
    btnEdit_ = new QPushButton("Edit", this);
    btnEdit_->setFixedHeight(22);
    btnEdit_->setCursor(Qt::PointingHandCursor);
    row1->addWidget(btnEdit_);
    v->addLayout(row1);

    v->addWidget(sensor_);
    v->addWidget(duty_);
    v->addWidget(temp_);

    connect(btnEdit_, &QPushButton::clicked, this, [this]{ emit editRequested(); });
}

void FanTile::setTitle(const QString& t)  { title_->setText(t); }
void FanTile::setSensor(const QString& s) { sensor_->setText(QString("sensor: %1").arg(s)); }
void FanTile::setDuty(double pct)         { duty_->setText(QString("duty: %1 %").arg(QString::number(pct, 'f', 0))); }
void FanTile::setTemp(double celsius)     { temp_->setText(QString("temp: %1 °C").arg(QString::number(celsius, 'f', 1))); }
