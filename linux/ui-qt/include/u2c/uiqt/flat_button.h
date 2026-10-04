// A flat rounded button drawn from the palette.
#pragma once
#include <QAbstractButton>

namespace u2c::uiqt {

class FlatButton : public QAbstractButton {
public:
    enum class Style { Normal, Accent };
    explicit FlatButton(QWidget* parent, Style style = Style::Normal);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    Style style_;
};

// A check box drawn from the palette. The standard Fusion one is almost invisible on the dark card, so the box gets
// its own border and a check mark when checked (not color alone).
class FlatCheck : public QAbstractButton {
public:
    explicit FlatCheck(QWidget* parent);

protected:
    void paintEvent(QPaintEvent* event) override;
};

}  // namespace u2c::uiqt
