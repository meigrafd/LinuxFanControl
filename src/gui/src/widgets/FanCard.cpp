#include "FanCard.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QComboBox>
#include <QSlider>
#include <QPushButton>

FanCard::FanCard(const Model& m, QWidget* parent) : QFrame(parent), m_(m) {
    setProperty("card", true);
    applyCardStyle();
    auto* v = new QVBoxLayout(this);

    auto* head = new QHBoxLayout();
    title_ = new QLabel(m_.name); title_->setStyleSheet("font-weight:600; font-size:16px;");
    head->addWidget(title_);
    head->addStretch();

    mode_ = new QComboBox(); mode_->addItems({"Auto", "Manual"});
    mode_->setCurrentText(m_.mode);
    head->addWidget(mode_);

    auto* btnEdit = new QPushButton(tr("Edit"));
    head->addWidget(btnEdit);
    v->addLayout(head);

    auto* g = new QGridLayout();
    lblSensor_ = new QLabel(m_.sensorPath);
    lblPwm_ = new QLabel(m_.pwmPath);
    lblTemp_ = new QLabel(QString::number(m_.lastTemp, 'f', 1) + "°C");
    lblOut_  = new QLabel(QString::number(m_.lastOut,  'f', 0) + "%");

    g->addWidget(new QLabel(tr("Sensor")), 0, 0); g->addWidget(lblSensor_, 0, 1);
    g->addWidget(new QLabel("PWM"), 1, 0); g->addWidget(lblPwm_, 1, 1);
    g->addWidget(new QLabel(tr("Temp")), 2, 0); g->addWidget(lblTemp_, 2, 1);
    g->addWidget(new QLabel(tr("Output")), 3, 0); g->addWidget(lblOut_, 3, 1);
    v->addLayout(g);

    slider_ = new QSlider(Qt::Orientation::Horizontal);
    slider_->setRange(0, 100);
    slider_->setValue(static_cast<int>(m_.manualPct));
    slider_->setEnabled(m_.mode == "Manual");
    v->addWidget(slider_);

    connect(btnEdit, &QPushButton::clicked, this, [this]{ emit editRequested(m_.id); });
    connect(mode_, &QComboBox::currentTextChanged, this, [this](const QString& text){
        slider_->setEnabled(text == "Manual");
        emit modeChanged(m_.id, text);
    });
    connect(slider_, &QSlider::valueChanged, this, [this](int v){
        emit manualChanged(m_.id, static_cast<double>(v));
    });
}

void FanCard::applyCardStyle() {
    setStyleSheet(R"(
    QFrame[card="true"] {
      border-radius: 12px;
      border: 1px solid rgba(0,0,0,0.12);
      padding: 12px;
      background: palette(base);
    }
    QPushButton { padding: 6px 10px; }
    )");
}

void FanCard::updateTelemetry(double tempC, double outPct) {
    lblTemp_->setText(QString::number(tempC, 'f', 1) + "°C");
    lblOut_->setText(QString::number(outPct, 'f', 0) + "%");
}

void FanCard::rename(const QString& newName) {
    title_->setText(newName);
}
