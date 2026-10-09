// Checks that every user-visible text fits its control in all languages at 100% scale.
// Not part of the normal build. Build and run:
//   cmake --build build --config Release --target LayoutCheck
//   build\Release\LayoutCheck.exe
// Exit code 0 = everything fits, 1 = at least one text is too wide.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <cstdio>
#include <string>
#include "../src/Localization.h"

using namespace Ultimate2CFixer;

namespace {

HDC g_dc = nullptr;
HFONT g_fontBody = nullptr;   // 11 pt, buttons / checkboxes / status line
HFONT g_fontSmall = nullptr;  // 9 pt, captions / battery text
int g_failures = 0;
int g_checked = 0;

HFONT MakeFont(int points, int weight) {
    return CreateFontW(-MulDiv(points, 96, 72), 0, 0, 0, weight, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                       OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
}

int Measure(HFONT font, const std::wstring& text) {
    HGDIOBJ old = SelectObject(g_dc, font);
    SIZE sz = {};
    GetTextExtentPoint32W(g_dc, text.c_str(), (int)text.length(), &sz);
    SelectObject(g_dc, old);
    return sz.cx;
}

void Check(const wchar_t* lang, const wchar_t* what, HFONT font, const std::wstring& text, int availableWidth) {
    ++g_checked;
    int w = Measure(font, text);
    if (w > availableWidth) {
        ++g_failures;
        wprintf(L"TOO WIDE [%s] %s: %d px > %d px  \"%s\"\n", lang, what, w, availableWidth, text.c_str());
    }
}

std::wstring Format(const std::wstring& fmt, int value) {
    wchar_t buf[256];
    swprintf_s(buf, fmt.c_str(), value);
    return buf;
}

void CheckLanguage(Language language, const wchar_t* name) {
    auto& loc = Localization::Instance();
    loc.SetLanguage(language);
    auto T = [&](StringId id) { return loc.Get(id); };

    // Sizes come from src/main.cpp (WM_CREATE / PaintDashboard). Buttons keep 12 px padding on each side,
    // checkboxes lose about 20 px for the box.
    const int kButtonPad = 16;

    // Top row
    Check(name, L"Start button", g_fontBody, T(StringId::StartBtn), 150 - kButtonPad);
    Check(name, L"Stop button", g_fontBody, T(StringId::StopBtn), 150 - kButtonPad);
    Check(name, L"Install driver button", g_fontBody, T(StringId::InstallDriverBtn), 150 - kButtonPad);
    Check(name, L"Driver download progress", g_fontBody, Format(T(StringId::DriverDownloadingShort), 100), 150 - kButtonPad);
    Check(name, L"Clear button", g_fontBody, T(StringId::ClearBtn), 64 - 8);

    // Status card (240 wide): caption, then dot + status line starting 30 px in
    Check(name, L"Status caption", g_fontSmall, T(StringId::StatusTitle), 240 - 32);
    for (StringId id : { StringId::StatusStopped, StringId::StatusConnected, StringId::StatusSearching, StringId::WaitingReconnect }) {
        Check(name, L"Status line", g_fontBody, T(id), 240 - 30 - 16);
    }
    Check(name, L"No device name", g_fontBody, T(StringId::NoDevice), 240 - 32);

    // Telemetry card (280 wide): caption sits left of the 66 px mode badge
    Check(name, L"Live input caption", g_fontSmall, T(StringId::LiveInputTitle), 280 - 80 - 16 - 8);
    Check(name, L"Idle readout", g_fontSmall, T(StringId::Idle), 280 - 80 - 16 - 8);
    Check(name, L"Rate readout", g_fontSmall, L"1000 Hz \u2022 1000.0 ms", 280 - 80 - 16 - 8);

    // Battery card (244 wide)
    Check(name, L"Battery caption", g_fontSmall, T(StringId::BatteryTitle), 244 - 32 - 60);
    for (StringId id : { StringId::BatteryLowStatus, StringId::BatteryModerateStatus, StringId::BatteryHealthyStatus, StringId::NoDevice }) {
        Check(name, L"Battery status", g_fontSmall, T(id), 244 - 32);
    }

    // Terminal card
    Check(name, L"Terminal caption", g_fontSmall, T(StringId::LogsTitle), 840 - 40 - 32 - 64 - 16);

    // Settings view
    for (StringId id : { StringId::StartWithWindows, StringId::MinimizeOnClose, StringId::AutoStartService,
                         StringId::LowBatteryNotification, StringId::NintendoMode, StringId::HairTrigger,
                         StringId::HideRealController }) {
        Check(name, L"Settings checkbox", g_fontBody, T(id), 650 - 20);
    }
    Check(name, L"Back button", g_fontBody, T(StringId::SettingsBack), 150 - kButtonPad);

    const int kWideButton = 340 - kButtonPad;
    for (StringId id : { StringId::DeadzoneOff, StringId::DeadzoneLow, StringId::DeadzoneNormal, StringId::DeadzoneHigh }) {
        Check(name, L"Deadzone button", g_fontBody, T(StringId::DeadzoneLabel) + L": " + T(id), kWideButton);
    }
    Check(name, L"Polling rate button", g_fontBody, T(StringId::PollingRateLabel) + L": 1000 Hz", kWideButton);
    for (StringId id : { StringId::CurveLinear, StringId::CurveSmooth, StringId::CurveAggressive }) {
        Check(name, L"Curve button", g_fontBody, T(StringId::CurveLabel) + L": " + T(id), kWideButton);
    }
}

} // namespace

int main() {
    g_dc = CreateCompatibleDC(nullptr);
    g_fontBody = MakeFont(11, FW_NORMAL);
    g_fontSmall = MakeFont(9, FW_NORMAL);

    CheckLanguage(Language::English, L"EN");
    CheckLanguage(Language::Turkish, L"TR");
    CheckLanguage(Language::Spanish, L"ES");

    wprintf(L"%d texts checked, %d too wide.\n", g_checked, g_failures);

    DeleteObject(g_fontBody);
    DeleteObject(g_fontSmall);
    DeleteDC(g_dc);
    return g_failures == 0 ? 0 : 1;
}
