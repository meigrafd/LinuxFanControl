#pragma once
#include <QWidget>
#include <QList>
#include <QPointF>
#include <QRectF>

class CurveEditor : public QWidget {
    Q_OBJECT
public:
    explicit CurveEditor(QWidget* parent=nullptr);
    QList<QPointF> points() const { return pts_; }
    void setPoints(const QList<QPointF>& p);

protected:
    void paintEvent(QPaintEvent*) override;

private:
    void ensureOrder();
    QPoint mapFromGraph(const QPointF& g) const; // graph coords (0..100) -> widget px
    QPointF mapToGraph(const QPoint& w) const;   // widget px -> graph coords (0..100)
private:
    QList<QPointF> pts_;
    QRectF graphRect_;
};
