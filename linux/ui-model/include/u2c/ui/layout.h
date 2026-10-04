// Window geometry in pixels at 100% scale. Client area 840 x 580. The settings screen has one extra row for the
// "hide the real controller" checkbox.
#pragma once

namespace u2c::ui {

struct Rect {
    int x, y, w, h;
    constexpr int right() const { return x + w; }
    constexpr int bottom() const { return y + h; }
};

inline constexpr int kWindowW = 840, kWindowH = 580;

inline constexpr int kTitleX = 24, kTitleY = 18;  // text positions are the top left corner of the text
inline constexpr Rect kLangButton{kWindowW - 160, 16, 38, 26};
inline constexpr Rect kSettingsButton{kWindowW - 112, 16, 38, 26};
inline constexpr Rect kTrayButton{kWindowW - 64, 16, 38, 26};

inline constexpr Rect kCardStatus{24, 56, 240, 96};
inline constexpr Rect kCardTelemetry{278, 56, 280, 96};
inline constexpr Rect kCardBattery{572, 56, kWindowW - 24 - 572, 96};
inline constexpr Rect kStartButton{24, 164, 150, 34};
inline constexpr Rect kStopButton{184, 164, 150, 34};
inline constexpr Rect kCardTerminal{20, 212, kWindowW - 40, kWindowH - 20 - 212};
inline constexpr Rect kClearButton{kWindowW - 88, 218, 54, 22};
inline constexpr Rect kLogBox{38, 248, kWindowW - 76, kWindowH - 20 - 248 - 12};
inline constexpr int kTerminalCaptionX = 36, kTerminalCaptionY = 224;

inline constexpr int kCaptionOffsetX = 16, kCaptionOffsetY = 14;  // card caption, small font
inline constexpr int kStatusNameOffsetY = 34;  // device name, title font
inline constexpr int kStatusLineOffsetY = 64;  // dot and status text
inline constexpr int kStatusTextOffsetX = 30;

// telemetry card (relative to the card)
inline constexpr int kVisOffsetX = 16, kVisOffsetY = 42;
inline constexpr int kStickBoxSize = 34, kStickBoxGap = 40, kStickCenter = 17, kStickTravel = 12, kDotRadius = 3;
inline constexpr int kAbxyOriginX = 88;
inline constexpr int kTileSize = 12;
// the four tiles: left, top, bottom, right (relative to kVisOffsetX + kAbxyOriginX, kVisOffsetY)
inline constexpr int kTileDx[4] = {2, 15, 15, 28}, kTileDy[4] = {11, 0, 22, 11};
inline constexpr Rect kBumper[2] = {{134, 8, 28, 18}, {168, 8, 28, 18}};  // LB, RB, relative to (kVisOffsetX, kVisOffsetY)
inline constexpr int kTriggerX[2] = {210, 223}, kTriggerY = 2, kTriggerW = 9, kTriggerH = 30;
inline constexpr Rect kBadge{-80 + 280, 12, 66, 14};  // right part of the telemetry card
inline constexpr int kRateTextOffsetY = 28;

inline constexpr int kBatteryPercentOffsetY = 10;
inline constexpr int kBatteryTrackOffsetY = 46, kBatteryTrackH = 8;
inline constexpr int kBatteryTextOffsetY = 64;

inline constexpr Rect kSettingsCard{24, 56, kWindowW - 48, kWindowH - 20 - 56};
inline constexpr int kSettingsTitleX = 44, kSettingsTitleY = 72;
inline constexpr int kCheckboxX = 44, kCheckboxW = 650, kCheckboxH = 22, kCheckboxFirstY = 106, kCheckboxStep = 30;
// checkboxes in order: start when I log in, minimize on close, auto-start service, low battery, Nintendo Mode,
// Hair Trigger, hide the real controller. The two button rows sit 30 px lower than on Windows because of the extra row.
inline constexpr int kCheckboxCount = 7;
inline constexpr Rect kDeadzoneButton{44, 326, 340, 36};
inline constexpr Rect kPollingButton{400, 326, 340, 36};
inline constexpr Rect kCurveButton{44, 374, 340, 36};
inline constexpr Rect kBackButton{400, 374, 150, 36};

constexpr int checkbox_y(int index) { return kCheckboxFirstY + index * kCheckboxStep; }

}  // namespace u2c::ui
