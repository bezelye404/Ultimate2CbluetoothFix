#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <dwmapi.h>

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif

namespace BitDoFixer {
namespace UI {

constexpr COLORREF ColorWindowBg          = RGB(15, 15, 18);     // #0F0F12
constexpr COLORREF ColorCardBg            = RGB(22, 22, 27);     // #16161B
constexpr COLORREF ColorCardBorder        = RGB(36, 36, 44);     // #24242C
constexpr COLORREF ColorTextPrimary       = RGB(244, 244, 245);  // #F4F4F5
constexpr COLORREF ColorTextSecondary     = RGB(161, 161, 170);  // #A1A1AA
constexpr COLORREF ColorTextMuted         = RGB(113, 113, 122);  // #71717A

constexpr COLORREF ColorStatusGreen       = RGB(16, 185, 129);   // #10B981 (Emerald)
constexpr COLORREF ColorStatusAmber       = RGB(245, 158, 11);   // #F59E0B (Amber)
constexpr COLORREF ColorStatusRed         = RGB(239, 68, 68);    // #EF4444 (Rose/Red)
constexpr COLORREF ColorStatusGray        = RGB(100, 116, 139);  // #64748B (Slate)

constexpr COLORREF ColorButtonBg          = RGB(28, 28, 34);     // #1C1C22
constexpr COLORREF ColorButtonHover       = RGB(38, 38, 46);     // #26262E
constexpr COLORREF ColorButtonBorder      = RGB(46, 46, 56);     // #2E2E38
constexpr COLORREF ColorButtonAccent      = RGB(36, 41, 54);     // #242936 (Refined Slate Dark)
constexpr COLORREF ColorButtonAccentHover = RGB(46, 52, 68);     // #2E3444
constexpr COLORREF ColorButtonAccentBorder= RGB(62, 71, 94);     // #3E475E
constexpr COLORREF ColorButtonAccentText  = RGB(248, 250, 252);  // #F8FAFC

inline void EnableImmersiveDarkMode(HWND hwnd) {
    BOOL useDarkMode = TRUE;
    DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &useDarkMode, sizeof(useDarkMode));
}

} // namespace UI
} // namespace BitDoFixer
