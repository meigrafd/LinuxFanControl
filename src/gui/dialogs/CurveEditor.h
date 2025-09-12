#pragma once
#include <QWidget>
#include <QVector>
#include <QPointF>

class CurveEditor : public QWidget {
    Q_OBJECT
public:
    explicit CurveEditor(QWidget* parent=nullptr);
    void setPoints(const QVector<QPointF>& pts);
    QVector<QPointF> points() const { return pts_; }
signals:
    void pointsChanged(QVector<QPointF> pts);
protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void mouseDoubleClickEvent(QMouseEvent*) override;
private:
    QVector<QPointF> pts_;
    int dragIdx_ = -1;
    QPoint mapToGraph(const QPoint& p) const;
    QPoint mapFromGraph(const QPoint& g) const;
    int nearestIdx(const QPoint& mouse) const;
    void ensureOrder();
};
