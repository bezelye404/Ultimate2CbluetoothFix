#include "u2c/uiqt/flat_button.h"

#include <QFont>
#include <QPainter>
#include <QPainterPath>

#include "u2c/ui/palette.h"
#include "u2c/uiqt/colors.h"

namespace u2c::uiqt {

FlatButton::FlatButton(QWidget* parent, Style style) : QAbstractButton(parent), style_(style) {
    setCursor(Qt::ArrowCursor);
}

void FlatButton::paintEvent(QPaintEvent*) {
    using ui::ColorId;
    const bool accent = style_ == Style::Accent;
    const bool pressed = isDown();
    const bool disabled = !isEnabled();

    ColorId bg = accent ? ColorId::AccentBg : ColorId::ButtonBg;
    ColorId border = accent ? ColorId::AccentBorder : ColorId::ButtonBorder;
    ColorId text = accent ? ColorId::AccentText : ColorId::TextPrimary;
    if (disabled) {
        bg = ColorId::DisabledBg;
        border = ColorId::DisabledBorder;
        text = ColorId::TextMuted;
    } else if (pressed) {
        bg = accent ? ColorId::AccentHover : ColorId::ButtonHover;
    }

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(QPen(qcolor(border), 1));
    p.setBrush(qcolor(bg));
    p.drawRoundedRect(QRectF(0.5, 0.5, width() - 1.0, height() - 1.0), 6, 6);

    QFont f = font();
    f.setPixelSize(15);
    p.setFont(f);
    p.setPen(qcolor(text));
    QRect r = rect();
    if (pressed) r.translate(0, 1);
    p.drawText(r, Qt::AlignCenter | Qt::TextSingleLine, this->text());
}

FlatCheck::FlatCheck(QWidget* parent) : QAbstractButton(parent) {
    setCheckable(true);
    setCursor(Qt::ArrowCursor);
}

void FlatCheck::paintEvent(QPaintEvent*) {
    using ui::ColorId;
    const bool on = isChecked();
    const bool disabled = !isEnabled();
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const int size = 16;
    const QRectF box(0.5, (height() - size) / 2.0 + 0.5, size - 1.0, size - 1.0);
    p.setPen(QPen(qcolor(on ? ColorId::Green : ColorId::TextMuted), 1));
    p.setBrush(qcolor(on ? ColorId::Green : ColorId::ButtonBg));
    p.drawRoundedRect(box, 3, 3);
    if (on) {
        QPen pen(qcolor(ColorId::OnGreen), 2);
        pen.setCapStyle(Qt::RoundCap);
        pen.setJoinStyle(Qt::RoundJoin);
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);
        QPainterPath check;
        check.moveTo(box.x() + 4, box.y() + 8);
        check.lineTo(box.x() + 7, box.y() + 11);
        check.lineTo(box.x() + 12, box.y() + 4.5);
        p.drawPath(check);
    }

    QFont f = font();
    f.setPixelSize(15);
    p.setFont(f);
    p.setPen(qcolor(disabled ? ColorId::TextMuted : ColorId::TextSecondary));
    p.drawText(QRect(size + 8, 0, width() - size - 8, height()), Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine, text());
}

}  // namespace u2c::uiqt
