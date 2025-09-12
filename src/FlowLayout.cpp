#include "FlowLayout.h"
#include <QWidget>
#include <algorithm>

FlowLayout::FlowLayout(QWidget* parent, int margin, int hSpacing, int vSpacing)
: QLayout(parent), hSpace_(hSpacing), vSpace_(vSpacing) {
    setContentsMargins(margin, margin, margin, margin);
}

FlowLayout::~FlowLayout() {
    QLayoutItem* item;
    while ((item = takeAt(0)))
        delete item;
}

void FlowLayout::addItem(QLayoutItem* item) { items_.append(item); }
int FlowLayout::count() const { return items_.size(); }
QLayoutItem* FlowLayout::itemAt(int i) const { return items_.value(i); }
QLayoutItem* FlowLayout::takeAt(int i) { return (i >= 0 && i < items_.size()) ? items_.takeAt(i) : nullptr; }
QSize FlowLayout::sizeHint() const { return minimumSize(); }
bool FlowLayout::hasHeightForWidth() const { return true; }

int FlowLayout::heightForWidth(int w) const {
    return doLayout(QRect(0,0,w,0), true);
}

QSize FlowLayout::minimumSize() const {
    QSize s;
    for (auto* item : items_) s = s.expandedTo(item->minimumSize());
    auto m = contentsMargins();
    s += QSize(2*m.left(), 2*m.top());
    return s;
}

void FlowLayout::setGeometry(const QRect& rect) {
    QLayout::setGeometry(rect);
    doLayout(rect, false);
}

int FlowLayout::doLayout(const QRect& rect, bool testOnly) const {
    int x = rect.x();
    int y = rect.y();
    int lineHeight = 0;

    for (auto* item : items_) {
        auto nextX = x + item->sizeHint().width() + hSpace_;
        if (nextX - hSpace_ > rect.right() && lineHeight > 0) {
            x = rect.x();
            y = y + lineHeight + vSpace_;
            nextX = x + item->sizeHint().width() + hSpace_;
            lineHeight = 0;
        }
        if (!testOnly)
            item->setGeometry(QRect(QPoint(x, y), item->sizeHint()));
        x = nextX;
        lineHeight = std::max(lineHeight, item->sizeHint().height());
    }
    return y + lineHeight - rect.y();
}
