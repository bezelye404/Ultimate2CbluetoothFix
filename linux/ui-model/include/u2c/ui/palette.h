// Colors. Flat and matte, no gradients. Saturation stays under about 55% (HSV) and text keeps at least 4.5:1 contrast
// against the backgrounds it is drawn on.
#pragma once
#include <cstdint>

namespace u2c::ui {

struct Rgb {
    uint8_t r, g, b;
    friend constexpr bool operator==(Rgb, Rgb) = default;
};

enum class ColorId {
    WindowBg, CardBg, CardBorder,
    TextPrimary, TextSecondary, TextMuted,
    Green, Amber, Red, Gray,
    ButtonBg, ButtonHover, ButtonBorder,
    AccentBg, AccentHover, AccentBorder, AccentText,
    TileBg, StickBg, BadgeBg, Cross, OnGreen,
    DisabledBg, DisabledBorder,
    Count
};

inline constexpr Rgb kPalette[static_cast<int>(ColorId::Count)] = {
    {0x0F, 0x0F, 0x12}, {0x16, 0x16, 0x1B}, {0x24, 0x24, 0x2C},      // window, card, border
    {0xF4, 0xF4, 0xF5}, {0xA1, 0xA1, 0xAA}, {0x8D, 0x8D, 0x97},  // text: primary, secondary, muted
    {0x5D, 0xBB, 0x8F}, {0xC9, 0xA2, 0x5A}, {0xDC, 0x7F, 0x78}, {0x87, 0x93, 0xA6},  // green, amber, red, gray
    {0x1C, 0x1C, 0x22}, {0x26, 0x26, 0x2E}, {0x2E, 0x2E, 0x38},      // buttons
    {0x24, 0x29, 0x36}, {0x2E, 0x34, 0x44}, {0x3E, 0x47, 0x5E}, {0xF8, 0xFA, 0xFC},    // accent buttons and their text
    {0x1C, 0x1C, 0x23}, {0x18, 0x18, 0x1E}, {0x20, 0x20, 0x28}, {0x28, 0x28, 0x32}, {0x0A, 0x14, 0x0F},   // tile, stick background, badge, crosshair, text on a lit tile
    {0x16, 0x16, 0x1A}, {0x20, 0x20, 0x26},                          // disabled button
};

constexpr Rgb color(ColorId id) { return kPalette[static_cast<int>(id)]; }

}  // namespace u2c::ui
