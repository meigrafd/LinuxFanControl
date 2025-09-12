#include "widgets/CollapsiblePanel.h"

#include <QToolButton>
#include <QFrame>
#include <QVBoxLayout>

CollapsiblePanel::CollapsiblePanel(const QString& title, QWidget* parent)
: QWidget(parent)
{
    auto* v = new QVBoxLayout(this);
    v->setContentsMargins(0,0,0,0);
    header_ = new QToolButton(this);
    header_->setText(title);
    header_->setCheckable(true);
    header_->setChecked(true);
    header_->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    header_->setArrowType(Qt::DownArrow);
    header_->setStyleSheet("QToolButton { font-weight:600; }");
    v->addWidget(header_);

    line_ = new QFrame(this);
    line_->setFrameShape(QFrame::HLine);
    line_->setFrameShadow(QFrame::Sunken);
    v->addWidget(line_);

    content_ = new QWidget(this);
    layContent_ = new QVBoxLayout(content_);
    layContent_->setContentsMargins(0,0,0,0);
    v->addWidget(content_);

    connect(header_, &QToolButton::toggled, this, [this](bool on){
        expanded_ = on;
        content_->setVisible(on);
        header_->setArrowType(on ? Qt::DownArrow : Qt::RightArrow);
    });
}

QWidget* CollapsiblePanel::body() const { return content_; }
void CollapsiblePanel::setExpanded(bool on) { header_->setChecked(on); }
bool CollapsiblePanel::isExpanded() const { return expanded_; }
