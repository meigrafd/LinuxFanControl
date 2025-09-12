#pragma once
// CollapsiblePanel - simple header+content expander used for Sensors section.

#include <QWidget>

class QToolButton;
class QFrame;
class QVBoxLayout;

class CollapsiblePanel : public QWidget {
    Q_OBJECT
public:
    explicit CollapsiblePanel(const QString& title, QWidget* parent = nullptr);

    QWidget* body() const;           // parent to add content
    void setExpanded(bool on);
    bool isExpanded() const;

private:
    QToolButton* header_{nullptr};
    QFrame*      line_{nullptr};
    QWidget*     content_{nullptr};
    QVBoxLayout* layContent_{nullptr};
    bool expanded_{true};
};
