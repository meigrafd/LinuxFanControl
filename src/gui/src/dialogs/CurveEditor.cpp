#include "CurveEditor.h"
#include <QPainter>
#include <algorithm>
#include <cmath>

CurveEditor::CurveEditor(QWidget* parent) : QWidget(parent) {
    setMinimumSize(320, 180);
    graphRect_ = QRectF(30, 10, 280, 140);
    pts_ = { QPointF(20,20), QPointF(40,40), QPointF(60,60), QPointF(80,100) };
    ensureOrder();
}

void CurveEditor::setPoints(const QList<QPointF>& p) {
    pts_ = p;
    ensureOrder();
    update();
}

void CurveEditor::ensureOrder() {
    std::sort(pts_.begin(), pts_.end(),
              [](const QPointF& a, const QPointF& b){ return a.x() < b.x(); });
}

void CurveEditor::paintEvent(QPaintEvent*) {
    QPainter g(this);
    g.setRenderHint(QPainter::Antialiasing, true);

    // axes
    g.drawRect(graphRect_);

    // polyline
    if (pts_.size() >= 2) {
        for (int i=1; i<pts_.size(); ++i) {
            QPoint a = mapFromGraph(pts_[i-1]);
            QPoint c = mapFromGraph(pts_[i]);
            g.drawLine(a, c);
        }
    }
    // points
    for (auto& p : pts_) {
        QPoint w = mapFromGraph(p);
        g.drawEllipse(QRect(w.x()-3, w.y()-3, 6, 6));
    }
}

QPoint CurveEditor::mapFromGraph(const QPointF& g) const {
    const double gx = std::clamp(g.x(), 0.0, 100.0);
    const double gy = std::clamp(g.y(), 0.0, 100.0);
    const double wx = graphRect_.left() + (gx/100.0)*graphRect_.width();
    const double wy = graphRect_.bottom() - (gy/100.0)*graphRect_.height();
    return QPoint(static_cast<int>(std::lround(wx)), static_cast<int>(std::lround(wy)));
}

QPointF CurveEditor::mapToGraph(const QPoint& w) const {
    const double gx = (w.x() - graphRect_.left()) / graphRect_.width() * 100.0;
    const double gy = (graphRect_.bottom() - w.y()) / graphRect_.height() * 100.0;
    return QPointF(std::clamp(gx,0.0,100.0), std::clamp(gy,0.0,100.0));
}
