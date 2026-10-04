// Palette colors as Qt colors.
#pragma once
#include <QColor>

#include "u2c/ui/palette.h"

namespace u2c::uiqt {

inline QColor qcolor(ui::ColorId id) {
    const ui::Rgb c = ui::color(id);
    return QColor(c.r, c.g, c.b);
}

}  // namespace u2c::uiqt
