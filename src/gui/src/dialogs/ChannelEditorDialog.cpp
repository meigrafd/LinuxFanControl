#include "ChannelEditorDialog.h"
#include "CurveEditor.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDialogButtonBox>
#include <QLineEdit>
#include <QDoubleSpinBox>
#include <QLabel>

ChannelEditorDialog::ChannelEditorDialog(const Model& m, QWidget* parent)
: QDialog(parent), m_(m) {
    setWindowTitle(tr("Edit Channel"));
    resize(720, 520);
    auto* v = new QVBoxLayout(this);

    auto* row = new QHBoxLayout();
    row->addWidget(new QLabel(tr("Label")));
    nameEdit_ = new QLineEdit(m_.name);
    row->addWidget(nameEdit_, 1);
    v->addLayout(row);

    editor_ = new CurveEditor(); editor_->setPoints(m_.curve);
    v->addWidget(editor_, 1);

    auto* h = new QHBoxLayout();
    spHyst_ = new QDoubleSpinBox(); spHyst_->setRange(0.0, 20.0); spHyst_->setSuffix(" °C"); spHyst_->setValue(m_.hyst);
    spTau_  = new QDoubleSpinBox(); spTau_->setRange(0.0, 60.0); spTau_->setSuffix(" s");  spTau_->setValue(m_.tau);
    h->addWidget(new QLabel(tr("Hysteresis"))); h->addWidget(spHyst_);
    h->addWidget(new QLabel(tr("Response time constant"))); h->addWidget(spTau_);
    h->addStretch(1);
    v->addLayout(h);

    auto* bb = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
    v->addWidget(bb);
    connect(bb, &QDialogButtonBox::accepted, this, &ChannelEditorDialog::onSave);
    connect(bb, &QDialogButtonBox::rejected, this, &ChannelEditorDialog::reject);
}

QString ChannelEditorDialog::newName() const { return nameEdit_->text().trimmed(); }

void ChannelEditorDialog::onSave() {
    emit saveRequested(m_.id, newName(), editor_->points(), spHyst_->value(), spTau_->value());
    accept();
}
