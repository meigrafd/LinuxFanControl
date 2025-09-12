#pragma once
#include <QLayout>
#include <QList>

class FlowLayout : public QLayout {
public:
    explicit FlowLayout(QWidget* parent = nullptr, int margin = 0, int hSpacing = 10, int vSpacing = 10);
    ~FlowLayout();

    void addItem(QLayoutItem* item) override;
    int count() const override;
    QLayoutItem* itemAt(int index) const override;
    QLayoutItem* takeAt(int index) override;
    QSize minimumSize() const override;
    QSize sizeHint() const override;
    void setGeometry(const QRect &rect) override;
    bool hasHeightForWidth() const override;
    int heightForWidth(int) const override;

private:
    int doLayout(const QRect& rect, bool testOnly) const;
    int hSpace_;
    int vSpace_;
    QList<QLayoutItem*> items_;
};
