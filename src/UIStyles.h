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

constexpr COLORREF ColorWindowBg        = RGB(18, 18, 20);     // #121214
constexpr COLORREF ColorCardBg          = RGB(24, 24, 28);     // #18181C
constexpr COLORREF ColorCardBorder      = RGB(39, 39, 46);     // #27272E
constexpr COLORREF ColorTextPrimary     = RGB(244, 244, 245);  // #F4F4F5
constexpr COLORREF ColorTextSecondary   = RGB(161, 161, 170);  // #A1A1AA
constexpr COLORREF ColorTextMuted       = RGB(113, 113, 122);  // #71717A

constexpr COLORREF ColorStatusGreen     = RGB(34, 197, 94);    // #22C55E
constexpr COLORREF ColorStatusAmber     = RGB(245, 158, 11);   // #F59E0B
constexpr COLORREF ColorStatusRed       = RGB(239, 68, 68);    // #EF4444
constexpr COLORREF ColorStatusGray      = RGB(113, 113, 122);  // #71717A

constexpr COLORREF ColorButtonBg          = RGB(32, 32, 38);     // #202026
constexpr COLORREF ColorButtonHover       = RGB(42, 42, 50);     // #2A2A32
constexpr COLORREF ColorButtonBorder      = RGB(52, 52, 62);     // #34343E
constexpr COLORREF ColorButtonAccent      = RGB(39, 39, 47);     // #27272F
constexpr COLORREF ColorButtonAccentHover = RGB(49, 49, 59);     // #31313B
constexpr COLORREF ColorButtonAccentBorder= RGB(66, 66, 78);     // #42424E
constexpr COLORREF ColorButtonAccentText  = RGB(244, 244, 245);  // #F4F4F5

inline void EnableImmersiveDarkMode(HWND hwnd) {
    BOOL useDarkMode = TRUE;
    DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &useDarkMode, sizeof(useDarkMode));
}

} // namespace UI
} // namespace BitDoFixer
