#include "CurveEditor.h"
#include <QPainter>
#include <QMouseEvent>
#include <algorithm>

static QPoint clampPt(const QPoint& p, int minv=0, int maxv=100) {
    return QPoint(std::clamp(p.x(), minv, maxv), std::clamp(p.y(), minv, maxv));
}

CurveEditor::CurveEditor(QWidget* parent) : QWidget(parent) {
    setMinimumSize(360, 220);
    setPoints({{20,20},{35,25},{50,50},{70,80}});
}

void CurveEditor::setPoints(const QVector<QPointF>& pts) {
    pts_.clear();
    for (auto& p : pts) pts_.push_back(QPoint((int)p.x(), (int)p.y()));
    ensureOrder();
    update();
}

void CurveEditor::ensureOrder() {
    std::sort(pts_.begin(), pts_.end(), [](const QPoint& a, const QPoint& b){ return a.x() < b.x(); });
}

QPoint CurveEditor::mapToGraph(const QPoint& p) const {
    const int lm=40, tm=10, rm=10, bm=30;
    int w = width()-lm-rm, h=height()-tm-bm;
    if (w<=0||h<=0) return QPoint(0,0);
    double x = 100.0*(p.x()-lm)/double(w);
    double y = 100.0*(h-(p.y()-tm))/double(h);
    return clampPt(QPoint((int)std::round(x), (int)std::round(y)));
}

QPoint CurveEditor::mapFromGraph(const QPoint& g) const {
    const int lm=40, tm=10, rm=10, bm=30;
    int w = width()-lm-rm, h=height()-tm-bm;
    if (w<=0||h<=0) return QPoint(lm, tm+h);
    double x = lm + (g.x()/100.0)*w;
    double y = tm + (1.0 - g.y()/100.0)*h;
    return QPoint((int)std::round(x), (int)std::round(y));
}

void CurveEditor::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.fillRect(rect(), palette().base());

    const int lm=40, tm=10, rm=10, bm=30;
    QRectF area(lm, tm, width()-lm-rm, height()-tm-bm);
    p.setPen(QPen(palette().mid().color(), 1));
    p.drawRect(area);

    p.setPen(QPen(palette().mid().color(), 1, Qt::DotLine));
    for (int x=0; x<=100; x+=10) {
        QPoint a = mapFromGraph(QPoint(x,0));
        QPoint b = mapFromGraph(QPoint(x,100));
        p.drawLine(a.x(), a.y(), b.x(), b.y());
    }
    for (int y=0; y<=100; y+=10) {
        QPoint a = mapFromGraph(QPoint(0,y));
        QPoint b = mapFromGraph(QPoint(100,y));
        p.drawLine(a.x(), a.y(), b.x(), b.y());
    }
    if (pts_.size()>=2) {
        p.setPen(QPen(palette().text().color(), 2));
        for (int i=1;i<pts_.size();++i) {
            QPoint a = mapFromGraph(pts_[i-1]);
            QPoint b = mapFromGraph(pts_[i]);
            p.drawLine(a, b);
        }
    }
    p.setBrush(palette().highlight());
    p.setPen(Qt::NoPen);
    for (int i=0;i<pts_.size();++i) {
        QPoint c = mapFromGraph(pts_[i]);
        p.drawEllipse(c, 6, 6);
    }
}

int CurveEditor::nearestIdx(const QPoint& mouse) const {
    int best = -1; int bestd = 1e9;
    for (int i=0;i<pts_.size();++i) {
        QPoint c = mapFromGraph(pts_[i]);
        int d = (c - mouse).manhattanLength();
        if (d < bestd) { bestd = d; best = i; }
    }
    return (bestd <= 18) ? best : -1;
}

void CurveEditor::mousePressEvent(QMouseEvent* ev) {
    if (ev->button() == Qt::RightButton) {
        if (pts_.size() > 2) {
            int idx = nearestIdx(ev->pos());
            if (idx >= 0) {
                pts_.removeAt(idx);
                update();
                emit pointsChanged(pts_);
            }
        }
        return;
    }
    if (ev->button() == Qt::LeftButton) {
        int idx = nearestIdx(ev->pos());
        if (idx >= 0) dragIdx_ = idx;
    }
}

void CurveEditor::mouseDoubleClickEvent(QMouseEvent* ev) {
    if (ev->button() == Qt::LeftButton) {
        QPoint g = mapToGraph(ev->pos());
        pts_.push_back(g);
        ensureOrder();
        update();
        emit pointsChanged(pts_);
    }
}

void CurveEditor::mouseMoveEvent(QMouseEvent* ev) {
    if (dragIdx_ < 0) return;
    QPoint g = mapToGraph(ev->pos());
    int left = (dragIdx_>0) ? pts_[dragIdx_-1].x()+1 : 0;
    int right= (dragIdx_<pts_.size()-1) ? pts_[dragIdx_+1].x()-1 : 100;
    g.setX(std::clamp(g.x(), left, right));
    pts_[dragIdx_] = g;
    update();
    emit pointsChanged(pts_);
}

void CurveEditor::mouseReleaseEvent(QMouseEvent*) { dragIdx_ = -1; }
