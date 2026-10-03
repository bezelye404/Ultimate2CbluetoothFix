#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif
#ifndef WINVER
#define WINVER 0x0A00
#endif
#ifndef NTDDI_VERSION
#define NTDDI_VERSION 0x0A000003
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>
#include <uxtheme.h>
#include <string>
#include <vector>
#include <deque>
#include <memory>
#include <algorithm>
#include "UIStyles.h"
#include "Localization.h"
#include "Remapper.h"
#include "BatteryMonitor.h"
#include "DriverInstaller.h"

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "uxtheme.lib")
#pragma comment(linker, "\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

using namespace Ultimate2CFixer;

namespace {

inline void InitDpiAwareness() {
    HMODULE hUser32 = GetModuleHandleW(L"user32.dll");
    if (hUser32) {
        typedef BOOL (WINAPI *pfn_SetDpi)(void*);
        auto fn = (pfn_SetDpi)GetProcAddress(hUser32, "SetProcessDpiAwarenessContext");
        if (fn) {
            fn((void*)(intptr_t)-4); // DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2
        }
    }
}

inline UINT SafeGetDpiForWindow(HWND hwnd) {
    HMODULE hUser32 = GetModuleHandleW(L"user32.dll");
    if (hUser32) {
        typedef UINT (WINAPI *pfn_GetDpi)(HWND);
        auto fn = (pfn_GetDpi)GetProcAddress(hUser32, "GetDpiForWindow");
        if (fn) return fn(hwnd);
    }
    return 96;
}

inline UINT SafeGetDpiForSystem() {
    HMODULE hUser32 = GetModuleHandleW(L"user32.dll");
    if (hUser32) {
        typedef UINT (WINAPI *pfn_GetDpiSys)();
        auto fn = (pfn_GetDpiSys)GetProcAddress(hUser32, "GetDpiForSystem");
        if (fn) return fn();
    }
    return 96;
}

constexpr int WM_TRAYICON           = WM_USER + 1;
constexpr int WM_UPDATE_LOG         = WM_USER + 2;
constexpr int WM_UPDATE_STATUS      = WM_USER + 3;
constexpr int WM_UPDATE_BATTERY         = WM_USER + 4;
constexpr int WM_UPDATE_INPUT           = WM_USER + 5;
constexpr int WM_DRIVER_INSTALL_DONE     = WM_USER + 6;
constexpr int WM_DRIVER_INSTALL_PROGRESS = WM_USER + 7;

constexpr int IDC_BTN_START         = 101;
constexpr int IDC_BTN_STOP          = 102;
constexpr int IDC_BTN_LANG          = 103;
constexpr int IDC_BTN_TRAY          = 104;
constexpr int IDC_BTN_CLEAR_LOGS    = 105;
constexpr int IDC_EDIT_LOGS         = 106;
constexpr int IDC_CHK_MINIMIZE_CLOSE= 107;
constexpr int IDC_BTN_SETTINGS      = 108;
constexpr int IDC_CHK_START_WINDOWS = 109;
constexpr int IDC_CHK_AUTO_START    = 110;
constexpr int IDC_CHK_LOW_BATTERY   = 111;
constexpr int IDC_BTN_DEADZONE      = 112;
constexpr int IDC_BTN_SETTINGS_BACK = 113;
constexpr int IDC_CHK_NINTENDO_MODE = 114;

constexpr int IDM_TRAY_OPEN         = 201;
constexpr int IDM_TRAY_EXIT         = 202;

constexpr size_t MAX_LOG_LINES      = 100;

HWND g_hWnd                         = nullptr;
HWND g_hBtnStart                    = nullptr;
HWND g_hBtnStop                     = nullptr;
HWND g_hBtnLang                     = nullptr;
HWND g_hBtnSettings                 = nullptr;
HWND g_hBtnTray                     = nullptr;
HWND g_hBtnClearLogs                = nullptr;
HWND g_hEditLogs                    = nullptr;

HWND g_hChkStartWindows             = nullptr;
HWND g_hChkMinimizeClose            = nullptr;
HWND g_hChkAutoStart                = nullptr;
HWND g_hChkLowBattery               = nullptr;
HWND g_hChkNintendoMode             = nullptr;
HWND g_hBtnDeadzone                 = nullptr;
HWND g_hBtnSettingsBack             = nullptr;

bool g_showSettings                 = false;
bool g_minimizeOnClose              = true;
bool g_startWithWindows             = false;
bool g_autoStartService             = true;
bool g_lowBatteryAlert              = true;
bool g_nintendoMode                 = false;
bool g_driverInstalled              = false;
bool g_driverInstalling             = false;
int g_deadzoneLevel                 = 2; // 0=0%, 1=8%, 2=12%, 3=20%
const int kDeadzoneValues[]         = { 0, 2600, 4000, 6500 };
bool g_batteryWarningSent           = false;
XUSB_REPORT g_liveInput             = {};

HFONT g_hFontTitle                  = nullptr;
HFONT g_hFontBody                   = nullptr;
HFONT g_hFontSmall                  = nullptr;
HFONT g_hFontMono                   = nullptr;

HBRUSH g_hBrWindowBg                = nullptr;
HBRUSH g_hBrCardBg                  = nullptr;
HBRUSH g_hBrEditBg                  = nullptr;

NOTIFYICONDATAW g_nid               = {};
bool g_trayCreated                  = false;
UINT g_uTaskbarRestartMsg           = 0;

std::unique_ptr<Remapper> g_remapper;
std::unique_ptr<BatteryMonitor> g_batteryMonitor;

std::deque<std::wstring> g_logLines;
std::wstring g_deviceName;
RemapperStatus g_currentStatus      = RemapperStatus::Stopped;
int g_batteryLevel                  = -1;
std::wstring g_batteryDevice;

int g_dpi = 96;
int S(int val) { return MulDiv(val, g_dpi, 96); }

void TrimWorkingSet() {
    SetProcessWorkingSetSize(GetCurrentProcess(), (SIZE_T)-1, (SIZE_T)-1);
}

void AppendLogMessage(const std::wstring& msg) {
    SYSTEMTIME st;
    GetLocalTime(&st);
    wchar_t timeBuf[32];
    swprintf_s(timeBuf, L"[%02d:%02d:%02d] ", st.wHour, st.wMinute, st.wSecond);

    g_logLines.push_back(std::wstring(timeBuf) + msg);
    while (g_logLines.size() > MAX_LOG_LINES) {
        g_logLines.pop_front();
    }

    std::wstring allLogs;
    for (const auto& line : g_logLines) {
        allLogs += line + L"\r\n";
    }

    SetWindowTextW(g_hEditLogs, allLogs.c_str());
    SendMessageW(g_hEditLogs, EM_SETSEL, (WPARAM)allLogs.length(), (LPARAM)allLogs.length());
    SendMessageW(g_hEditLogs, EM_SCROLLCARET, 0, 0);
}

bool CheckStartWithWindows() {
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        DWORD type = 0;
        DWORD size = 0;
        LONG res = RegQueryValueExW(hKey, L"Ultimate2CFixer", NULL, &type, NULL, &size);
        if (res != ERROR_SUCCESS) {
            res = RegQueryValueExW(hKey, L"8BitDoUltimate2CFixer", NULL, &type, NULL, &size);
        }
        RegCloseKey(hKey);
        return (res == ERROR_SUCCESS);
    }
    return false;
}

void ApplyStartWithWindows(bool enable) {
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
        RegDeleteValueW(hKey, L"8BitDoUltimate2CFixer");
        if (enable) {
            wchar_t path[MAX_PATH];
            GetModuleFileNameW(NULL, path, MAX_PATH);
            std::wstring cmd = L"\"" + std::wstring(path) + L"\" --minimized";
            RegSetValueExW(hKey, L"Ultimate2CFixer", 0, REG_SZ, (const BYTE*)cmd.c_str(), (DWORD)((cmd.length() + 1) * sizeof(wchar_t)));
        } else {
            RegDeleteValueW(hKey, L"Ultimate2CFixer");
        }
        RegCloseKey(hKey);
    }
}

void UpdateDeadzoneButtonText() {
    auto& loc = Localization::Instance();
    std::wstring dzText = loc.Get(StringId::DeadzoneLabel) + L": ";
    switch (g_deadzoneLevel) {
        case 0: dzText += loc.Get(StringId::DeadzoneOff); break;
        case 1: dzText += loc.Get(StringId::DeadzoneLow); break;
        case 2: dzText += loc.Get(StringId::DeadzoneNormal); break;
        case 3: dzText += loc.Get(StringId::DeadzoneHigh); break;
    }
    SetWindowTextW(g_hBtnDeadzone, dzText.c_str());
}

void SwitchView(bool showSettings) {
    g_showSettings = showSettings;
    int showMain = showSettings ? SW_HIDE : SW_SHOW;
    int showSet = showSettings ? SW_SHOW : SW_HIDE;

    ShowWindow(g_hBtnStart, showMain);
    ShowWindow(g_hBtnStop, showMain);
    ShowWindow(g_hBtnClearLogs, showMain);
    ShowWindow(g_hEditLogs, showMain);

    ShowWindow(g_hChkStartWindows, showSet);
    ShowWindow(g_hChkMinimizeClose, showSet);
    ShowWindow(g_hChkAutoStart, showSet);
    ShowWindow(g_hChkLowBattery, showSet);
    ShowWindow(g_hChkNintendoMode, showSet);
    ShowWindow(g_hBtnDeadzone, showSet);
    ShowWindow(g_hBtnSettingsBack, showSet);

    InvalidateRect(g_hWnd, NULL, TRUE);
}

void UpdateTrayTooltip() {
    if (!g_trayCreated) return;
    auto& loc = Localization::Instance();
    std::wstring tip = L"Ultimate2CFixer";
    if (g_currentStatus == RemapperStatus::Connected) {
        tip += L": " + loc.Get(StringId::StatusConnected);
        if (g_batteryLevel >= 0) {
            tip += L" (" + std::to_wstring(g_batteryLevel) + L"%)";
        }
    } else if (g_currentStatus == RemapperStatus::Searching) {
        tip += L": " + loc.Get(StringId::StatusSearching);
    } else {
        tip += L": " + loc.Get(StringId::StatusStopped);
    }
    wcsncpy_s(g_nid.szTip, tip.c_str(), _TRUNCATE);
    g_nid.uFlags = NIF_TIP;
    Shell_NotifyIconW(NIM_MODIFY, &g_nid);
    g_nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
}

void UpdateUIStrings() {
    auto& loc = Localization::Instance();
    SetWindowTextW(g_hWnd, loc.Get(StringId::AppTitle).c_str());
    if (!g_driverInstalled) {
        SetWindowTextW(g_hBtnStart, loc.Get(StringId::InstallDriverBtn).c_str());
    } else {
        SetWindowTextW(g_hBtnStart, (g_remapper && g_remapper->IsRunning()) ? loc.Get(StringId::StopBtn).c_str() : loc.Get(StringId::StartBtn).c_str());
    }
    SetWindowTextW(g_hBtnStop, loc.Get(StringId::StopBtn).c_str());
    SetWindowTextW(g_hBtnClearLogs, loc.Get(StringId::ClearBtn).c_str());
    SetWindowTextW(g_hBtnLang, loc.IsEnglish() ? L"TR" : L"EN");
    SetWindowTextW(g_hChkStartWindows, loc.Get(StringId::StartWithWindows).c_str());
    SetWindowTextW(g_hChkMinimizeClose, loc.Get(StringId::MinimizeOnClose).c_str());
    SetWindowTextW(g_hChkAutoStart, loc.Get(StringId::AutoStartService).c_str());
    SetWindowTextW(g_hChkLowBattery, loc.Get(StringId::LowBatteryNotification).c_str());
    SetWindowTextW(g_hChkNintendoMode, loc.Get(StringId::NintendoMode).c_str());
    SetWindowTextW(g_hBtnSettingsBack, loc.Get(StringId::SettingsBack).c_str());
    UpdateDeadzoneButtonText();
    UpdateTrayTooltip();
    InvalidateRect(g_hWnd, NULL, TRUE);
}

void TriggerDriverInstall() {
    if (g_driverInstalling) return;
    g_driverInstalling = true;
    EnableWindow(g_hBtnStart, FALSE);
    auto& loc = Localization::Instance();
    AppendLogMessage(loc.Get(StringId::DriverInstalling));
    StartViGEmBusInstall(
        g_hWnd,
        [](int pct, const std::wstring&) {
            PostMessageW(g_hWnd, WM_DRIVER_INSTALL_PROGRESS, (WPARAM)pct, 0);
        },
        [](bool success, const std::wstring&) {
            PostMessageW(g_hWnd, WM_DRIVER_INSTALL_DONE, (WPARAM)(success ? 1 : 0), 0);
        }
    );
}

void StartServices() {
    if (!g_driverInstalled) {
        TriggerDriverInstall();
        return;
    }
    if (g_remapper && g_remapper->IsRunning()) return;

    EnableWindow(g_hBtnStart, FALSE);
    EnableWindow(g_hBtnStop, TRUE);

    auto& loc = Localization::Instance();
    AppendLogMessage(loc.Get(StringId::LogServicesStarting));

    g_currentStatus = RemapperStatus::Searching;
    g_deviceName = loc.Get(StringId::StatusSearching);
    InvalidateRect(g_hWnd, NULL, FALSE);

    g_remapper = std::make_unique<Remapper>();
    g_remapper->SetDeadzone(kDeadzoneValues[g_deadzoneLevel]);
    g_remapper->SetNintendoMode(g_nintendoMode);
    g_remapper->Start(
        g_hWnd,
        [](const std::wstring& msg) {
            auto* pMsg = new std::wstring(msg);
            PostMessageW(g_hWnd, WM_UPDATE_LOG, (WPARAM)pMsg, 0);
        },
        [](RemapperStatus status, const std::wstring& devName) {
            auto* pName = new std::wstring(devName);
            PostMessageW(g_hWnd, WM_UPDATE_STATUS, (WPARAM)status, (LPARAM)pName);
        },
        [](const XUSB_REPORT& report) {
            g_liveInput = report;
            PostMessageW(g_hWnd, WM_UPDATE_INPUT, 0, 0);
        }
    );

    g_batteryMonitor = std::make_unique<BatteryMonitor>();
    g_batteryMonitor->Start(
        [](const std::wstring& msg) {
            auto* pMsg = new std::wstring(msg);
            PostMessageW(g_hWnd, WM_UPDATE_LOG, (WPARAM)pMsg, 0);
        },
        [](const std::wstring& devName, int level) {
            auto* pName = new std::wstring(devName);
            PostMessageW(g_hWnd, WM_UPDATE_BATTERY, (WPARAM)level, (LPARAM)pName);
        }
    );
}

void StopServices() {
    if (g_remapper) {
        g_remapper->Stop();
        g_remapper.reset();
    }
    if (g_batteryMonitor) {
        g_batteryMonitor->Stop();
        g_batteryMonitor.reset();
    }

    EnableWindow(g_hBtnStart, TRUE);
    EnableWindow(g_hBtnStop, FALSE);

    auto& loc = Localization::Instance();
    AppendLogMessage(loc.Get(StringId::LogServicesStopping));

    g_currentStatus = RemapperStatus::Stopped;
    g_deviceName = loc.Get(StringId::NoDevice);
    g_batteryLevel = -1;
    g_batteryDevice.clear();

    InvalidateRect(g_hWnd, NULL, FALSE);
    TrimWorkingSet();
}

void SetupTray(HWND hwnd) {
    if (g_trayCreated) {
        Shell_NotifyIconW(NIM_DELETE, &g_nid);
        g_trayCreated = false;
    }

    memset(&g_nid, 0, sizeof(NOTIFYICONDATAW));
    g_nid.cbSize = sizeof(NOTIFYICONDATAW);
    g_nid.hWnd = hwnd;
    g_nid.uID = 1001;
    g_nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    g_nid.uCallbackMessage = WM_TRAYICON;

    int cxSm = GetSystemMetrics(SM_CXSMICON);
    int cySm = GetSystemMetrics(SM_CYSMICON);
    HICON hSm = (HICON)LoadImageW(
        GetModuleHandleW(NULL),
        MAKEINTRESOURCEW(1),
        IMAGE_ICON,
        cxSm,
        cySm,
        LR_DEFAULTCOLOR | LR_SHARED
    );
    if (!hSm) {
        hSm = LoadIconW(GetModuleHandleW(NULL), MAKEINTRESOURCEW(1));
    }
    if (!hSm) {
        hSm = LoadIconW(NULL, (LPCWSTR)IDI_APPLICATION);
    }
    g_nid.hIcon = hSm;
    wcscpy_s(g_nid.szTip, L"Ultimate2CFixer");
    g_trayCreated = Shell_NotifyIconW(NIM_ADD, &g_nid);
    if (g_trayCreated) {
        g_nid.uVersion = NOTIFYICON_VERSION_4;
        Shell_NotifyIconW(NIM_SETVERSION, &g_nid);
    }
}

void MinimizeToTray() {
    ShowWindow(g_hWnd, SW_HIDE);
    TrimWorkingSet();
}

void RestoreFromTray() {
    ShowWindow(g_hWnd, SW_SHOW);
    ShowWindow(g_hWnd, SW_RESTORE);
    SetForegroundWindow(g_hWnd);
}

void ShowTrayMenu() {
    POINT pt;
    GetCursorPos(&pt);
    HMENU hMenu = CreatePopupMenu();
    auto& loc = Localization::Instance();
    AppendMenuW(hMenu, MF_STRING, IDM_TRAY_OPEN, loc.Get(StringId::TrayOpen).c_str());
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hMenu, MF_STRING, IDM_TRAY_EXIT, loc.Get(StringId::TrayExit).c_str());

    SetForegroundWindow(g_hWnd);
    TrackPopupMenu(hMenu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, g_hWnd, NULL);
    DestroyMenu(hMenu);
}

void DrawCard(HDC hdc, const RECT& rc) {
    HPEN hPen = CreatePen(PS_SOLID, 1, UI::ColorCardBorder);
    HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);
    HBRUSH hOldBr = (HBRUSH)SelectObject(hdc, g_hBrCardBg);

    RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, S(8), S(8));

    SelectObject(hdc, hOldBr);
    SelectObject(hdc, hOldPen);
    DeleteObject(hPen);
}

void DrawModernButton(LPDRAWITEMSTRUCT dis, bool isAccent = false) {
    HDC hdc = dis->hDC;
    RECT rc = dis->rcItem;
    bool isPressed = (dis->itemState & ODS_SELECTED);
    bool isDisabled = (dis->itemState & ODS_DISABLED);

    // MARK: Fill button background with parent color to eliminate white corner notches
    HBRUSH hParentBg = (dis->hwndItem == g_hBtnClearLogs) ? g_hBrCardBg : g_hBrWindowBg;
    FillRect(hdc, &rc, hParentBg);

    COLORREF bg = isAccent ? UI::ColorButtonAccent : UI::ColorButtonBg;
    COLORREF border = isAccent ? UI::ColorButtonAccentBorder : UI::ColorButtonBorder;
    COLORREF text = isAccent ? UI::ColorButtonAccentText : UI::ColorTextPrimary;

    if (isDisabled) {
        bg = RGB(22, 22, 26);
        border = RGB(32, 32, 38);
        text = UI::ColorTextMuted;
    } else if (isPressed) {
        bg = isAccent ? UI::ColorButtonAccentHover : UI::ColorButtonHover;
    }

    HBRUSH hBr = CreateSolidBrush(bg);
    HPEN hPen = CreatePen(PS_SOLID, 1, border);
    HBRUSH hOldBr = (HBRUSH)SelectObject(hdc, hBr);
    HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);

    RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, S(6), S(6));

    SelectObject(hdc, hOldBr);
    SelectObject(hdc, hOldPen);
    DeleteObject(hBr);
    DeleteObject(hPen);

    wchar_t textBuf[128];
    GetWindowTextW(dis->hwndItem, textBuf, 128);

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, text);
    SelectObject(hdc, g_hFontBody);

    if (isPressed) {
        OffsetRect(&rc, 0, 1);
    }
    DrawTextW(hdc, textBuf, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

void PaintDashboard(HWND hwnd, HDC hdc) {
    RECT clientRc;
    GetClientRect(hwnd, &clientRc);

    // Double buffering
    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP memBitmap = CreateCompatibleBitmap(hdc, clientRc.right, clientRc.bottom);
    HBITMAP oldBmp = (HBITMAP)SelectObject(memDC, memBitmap);

    FillRect(memDC, &clientRc, g_hBrWindowBg);
    SetBkMode(memDC, TRANSPARENT);

    auto& loc = Localization::Instance();

    if (g_showSettings) {
        // MARK: Settings View Card
        RECT cardSettings = { S(20), S(56), clientRc.right - S(20), clientRc.bottom - S(20) };
        DrawCard(memDC, cardSettings);

        SelectObject(memDC, g_hFontTitle);
        SetTextColor(memDC, UI::ColorTextPrimary);
        TextOutW(memDC, S(36), S(72), loc.Get(StringId::SettingsTitle).c_str(), (int)loc.Get(StringId::SettingsTitle).length());

        BitBlt(hdc, 0, 0, clientRc.right, clientRc.bottom, memDC, 0, 0, SRCCOPY);
        SelectObject(memDC, oldBmp);
        DeleteObject(memBitmap);
        DeleteDC(memDC);
        return;
    }

    // MARK: Header
    SelectObject(memDC, g_hFontTitle);
    SetTextColor(memDC, UI::ColorTextPrimary);
    TextOutW(memDC, S(20), S(18), loc.Get(StringId::AppTitle).c_str(), (int)loc.Get(StringId::AppTitle).length());

    int gap = S(12);
    int topCardTop = S(56);
    int topCardBottom = S(152);

    // MARK: Card 1 - Controller Status Card
    RECT cardStatus = { S(20), topCardTop, S(260), topCardBottom };
    DrawCard(memDC, cardStatus);

    SelectObject(memDC, g_hFontSmall);
    SetTextColor(memDC, UI::ColorTextMuted);
    TextOutW(memDC, cardStatus.left + S(16), cardStatus.top + S(14), loc.Get(StringId::StatusTitle).c_str(), (int)loc.Get(StringId::StatusTitle).length());

    SelectObject(memDC, g_hFontTitle);
    SetTextColor(memDC, UI::ColorTextPrimary);
    std::wstring devName = (g_currentStatus == RemapperStatus::Connected && !g_deviceName.empty()) ? g_deviceName : loc.Get(StringId::NoDevice);
    RECT devNameRc = { cardStatus.left + S(16), cardStatus.top + S(34), cardStatus.right - S(16), cardStatus.top + S(58) };
    DrawTextW(memDC, devName.c_str(), (int)devName.length(), &devNameRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    // Status Dot
    COLORREF dotColor = UI::ColorStatusGray;
    std::wstring statusText = loc.Get(StringId::StatusStopped);
    if (g_currentStatus == RemapperStatus::Connected) {
        dotColor = UI::ColorStatusGreen;
        statusText = loc.Get(StringId::StatusConnected);
    } else if (g_currentStatus == RemapperStatus::Searching) {
        dotColor = UI::ColorStatusAmber;
        statusText = loc.Get(StringId::StatusSearching);
    } else if (g_currentStatus == RemapperStatus::Disconnected) {
        dotColor = UI::ColorStatusRed;
        statusText = loc.Get(StringId::WaitingReconnect);
    }

    SelectObject(memDC, g_hFontBody);
    SetTextColor(memDC, dotColor);
    TextOutW(memDC, cardStatus.left + S(16), cardStatus.top + S(64), L"●", 1);

    SetTextColor(memDC, UI::ColorTextSecondary);
    TextOutW(memDC, cardStatus.left + S(30), cardStatus.top + S(64), statusText.c_str(), (int)statusText.length());

    // MARK: Card 2 - Live Input Telemetry Card
    RECT cardTelemetry = { cardStatus.right + gap, topCardTop, cardStatus.right + gap + S(240), topCardBottom };
    DrawCard(memDC, cardTelemetry);

    SelectObject(memDC, g_hFontSmall);
    SetTextColor(memDC, UI::ColorTextMuted);
    TextOutW(memDC, cardTelemetry.left + S(16), cardTelemetry.top + S(14), loc.Get(StringId::LiveInputTitle).c_str(), (int)loc.Get(StringId::LiveInputTitle).length());

    // Mode Badge Pill
    std::wstring badgeText = g_nintendoMode ? L"NINTENDO" : L"XBOX";
    RECT badgeRc = { cardTelemetry.right - S(82), cardTelemetry.top + S(12), cardTelemetry.right - S(16), cardTelemetry.top + S(28) };
    HBRUSH hBadgeBg = CreateSolidBrush(RGB(32, 32, 40));
    HPEN hBadgePen = CreatePen(PS_SOLID, 1, UI::ColorCardBorder);
    HBRUSH hOldBr = (HBRUSH)SelectObject(memDC, hBadgeBg);
    HPEN hOldPn = (HPEN)SelectObject(memDC, hBadgePen);
    RoundRect(memDC, badgeRc.left, badgeRc.top, badgeRc.right, badgeRc.bottom, S(4), S(4));
    SelectObject(memDC, hOldBr);
    SelectObject(memDC, hOldPn);
    DeleteObject(hBadgeBg);
    DeleteObject(hBadgePen);

    SetTextColor(memDC, g_nintendoMode ? UI::ColorStatusAmber : UI::ColorStatusGreen);
    DrawTextW(memDC, badgeText.c_str(), (int)badgeText.length(), &badgeRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    // Left Stick & Right Stick Boxes
    int visX = cardTelemetry.left + S(16);
    int visY = cardTelemetry.top + S(42);

    RECT lsBox = { visX, visY, visX + S(34), visY + S(34) };
    RECT rsBox = { visX + S(40), visY, visX + S(74), visY + S(34) };
    HBRUSH hStickBg = CreateSolidBrush(RGB(24, 24, 30));
    HPEN hStickPen = CreatePen(PS_SOLID, 1, UI::ColorCardBorder);
    hOldBr = (HBRUSH)SelectObject(memDC, hStickBg);
    hOldPn = (HPEN)SelectObject(memDC, hStickPen);
    RoundRect(memDC, lsBox.left, lsBox.top, lsBox.right, lsBox.bottom, S(4), S(4));
    RoundRect(memDC, rsBox.left, rsBox.top, rsBox.right, rsBox.bottom, S(4), S(4));

    // Stick crosshairs
    HPEN hCrossPen = CreatePen(PS_SOLID, 1, RGB(40, 40, 50));
    SelectObject(memDC, hCrossPen);
    MoveToEx(memDC, lsBox.left + S(17), lsBox.top + S(6), NULL);
    LineTo(memDC, lsBox.left + S(17), lsBox.bottom - S(6));
    MoveToEx(memDC, lsBox.left + S(6), lsBox.top + S(17), NULL);
    LineTo(memDC, lsBox.right - S(6), lsBox.top + S(17));

    MoveToEx(memDC, rsBox.left + S(17), rsBox.top + S(6), NULL);
    LineTo(memDC, rsBox.left + S(17), rsBox.bottom - S(6));
    MoveToEx(memDC, rsBox.left + S(6), rsBox.top + S(17), NULL);
    LineTo(memDC, rsBox.right - S(6), rsBox.top + S(17));
    DeleteObject(hCrossPen);

    SelectObject(memDC, hOldBr);
    SelectObject(memDC, hOldPn);
    DeleteObject(hStickBg);
    DeleteObject(hStickPen);

    // Live Stick Dots
    int lsDotX = lsBox.left + S(17) + (g_liveInput.sThumbLX * S(12)) / 32768;
    int lsDotY = lsBox.top + S(17) - (g_liveInput.sThumbLY * S(12)) / 32768;
    int rsDotX = rsBox.left + S(17) + (g_liveInput.sThumbRX * S(12)) / 32768;
    int rsDotY = rsBox.top + S(17) - (g_liveInput.sThumbRY * S(12)) / 32768;

    HBRUSH hDotBr = CreateSolidBrush(UI::ColorStatusGreen);
    HPEN hNullP = CreatePen(PS_NULL, 0, 0);
    SelectObject(memDC, hDotBr);
    SelectObject(memDC, hNullP);
    Ellipse(memDC, lsDotX - S(3), lsDotY - S(3), lsDotX + S(3), lsDotY + S(3));
    Ellipse(memDC, rsDotX - S(3), rsDotY - S(3), rsDotX + S(3), rsDotY + S(3));
    DeleteObject(hDotBr);
    DeleteObject(hNullP);

    // Live Dynamic ABXY Diamond (Real-time Nintendo vs Xbox layout)
    int diaX = visX + S(88);
    struct BtnDef { const wchar_t* lbl; USHORT m; int x; int y; };
    BtnDef bArr[4];
    if (g_nintendoMode) {
        // Nintendo Mode: Top X, Bottom B, Left Y, Right A
        bArr[0] = { L"Y", XUSB_GAMEPAD_X, diaX + S(2), visY + S(11) };   // Left
        bArr[1] = { L"X", XUSB_GAMEPAD_Y, diaX + S(15), visY + S(0) };   // Top
        bArr[2] = { L"B", XUSB_GAMEPAD_A, diaX + S(15), visY + S(22) };  // Bottom
        bArr[3] = { L"A", XUSB_GAMEPAD_B, diaX + S(28), visY + S(11) };  // Right
    } else {
        // Xbox Mode: Top Y, Bottom A, Left X, Right B
        bArr[0] = { L"X", XUSB_GAMEPAD_X, diaX + S(2), visY + S(11) };   // Left
        bArr[1] = { L"Y", XUSB_GAMEPAD_Y, diaX + S(15), visY + S(0) };   // Top
        bArr[2] = { L"A", XUSB_GAMEPAD_A, diaX + S(15), visY + S(22) };  // Bottom
        bArr[3] = { L"B", XUSB_GAMEPAD_B, diaX + S(28), visY + S(11) };  // Right
    }

    SelectObject(memDC, g_hFontSmall);
    SetBkMode(memDC, TRANSPARENT);
    for (const auto& b : bArr) {
        bool on = (g_liveInput.wButtons & b.m) != 0;
        HBRUSH hb = CreateSolidBrush(on ? UI::ColorStatusGreen : RGB(28, 28, 35));
        HPEN hp = CreatePen(PS_SOLID, 1, on ? UI::ColorStatusGreen : UI::ColorCardBorder);
        hOldBr = (HBRUSH)SelectObject(memDC, hb);
        hOldPn = (HPEN)SelectObject(memDC, hp);
        RECT brc = { b.x, b.y, b.x + S(12), b.y + S(12) };
        RoundRect(memDC, brc.left, brc.top, brc.right, brc.bottom, S(3), S(3));
        SelectObject(memDC, hOldBr);
        SelectObject(memDC, hOldPn);
        DeleteObject(hb);
        DeleteObject(hp);
        SetTextColor(memDC, on ? RGB(10, 20, 15) : UI::ColorTextMuted);
        DrawTextW(memDC, b.lbl, 1, &brc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    // LT / RT Analog Trigger Meters
    int trigX = cardTelemetry.right - S(38);
    RECT ltRc = { trigX, visY + S(2), trigX + S(9), visY + S(32) };
    RECT rtRc = { trigX + S(15), visY + S(2), trigX + S(24), visY + S(32) };
    HBRUSH htbg = CreateSolidBrush(RGB(24, 24, 30));
    FillRect(memDC, &ltRc, htbg);
    FillRect(memDC, &rtRc, htbg);
    DeleteObject(htbg);

    if (g_liveInput.bLeftTrigger > 0) {
        int fh = (g_liveInput.bLeftTrigger * S(30)) / 255;
        RECT frc = { ltRc.left, ltRc.bottom - fh, ltRc.right, ltRc.bottom };
        HBRUSH hf = CreateSolidBrush(UI::ColorStatusGreen);
        FillRect(memDC, &frc, hf);
        DeleteObject(hf);
    }
    if (g_liveInput.bRightTrigger > 0) {
        int fh = (g_liveInput.bRightTrigger * S(30)) / 255;
        RECT frc = { rtRc.left, rtRc.bottom - fh, rtRc.right, rtRc.bottom };
        HBRUSH hf = CreateSolidBrush(UI::ColorStatusGreen);
        FillRect(memDC, &frc, hf);
        DeleteObject(hf);
    }

    // MARK: Card 3 - Battery Card
    RECT cardBattery = { cardTelemetry.right + gap, topCardTop, clientRc.right - S(20), topCardBottom };
    DrawCard(memDC, cardBattery);

    SelectObject(memDC, g_hFontSmall);
    SetTextColor(memDC, UI::ColorTextMuted);
    TextOutW(memDC, cardBattery.left + S(14), cardBattery.top + S(14), loc.Get(StringId::BatteryTitle).c_str(), (int)loc.Get(StringId::BatteryTitle).length());

    std::wstring pctStr = (g_batteryLevel >= 0) ? (std::to_wstring(g_batteryLevel) + L"%") : L"--%";
    SelectObject(memDC, g_hFontTitle);
    SetTextColor(memDC, UI::ColorTextPrimary);
    RECT pctRc = { cardBattery.left + S(14), cardBattery.top + S(12), cardBattery.right - S(14), cardBattery.top + S(32) };
    DrawTextW(memDC, pctStr.c_str(), (int)pctStr.length(), &pctRc, DT_RIGHT | DT_SINGLELINE);

    // Battery Bar
    RECT trackRc = { cardBattery.left + S(14), cardBattery.top + S(46), cardBattery.right - S(14), cardBattery.top + S(52) };
    HBRUSH hTrackBr = CreateSolidBrush(UI::ColorCardBorder);
    FillRect(memDC, &trackRc, hTrackBr);
    DeleteObject(hTrackBr);

    if (g_batteryLevel > 0) {
        int trackWidth = trackRc.right - trackRc.left;
        int fillWidth = (trackWidth * (std::min)(g_batteryLevel, 100)) / 100;
        RECT fillRc = { trackRc.left, trackRc.top, trackRc.left + fillWidth, trackRc.bottom };
        COLORREF fillCol = (g_batteryLevel <= 20) ? UI::ColorStatusRed : (g_batteryLevel <= 50 ? UI::ColorStatusAmber : UI::ColorStatusGreen);
        HBRUSH hFillBr = CreateSolidBrush(fillCol);
        FillRect(memDC, &fillRc, hFillBr);
        DeleteObject(hFillBr);
    }

    SelectObject(memDC, g_hFontSmall);
    SetTextColor(memDC, UI::ColorTextMuted);
    std::wstring bDevName = (g_batteryLevel >= 0 && !g_batteryDevice.empty()) ? g_batteryDevice : loc.Get(StringId::NoDevice);
    RECT bDevRc = { cardBattery.left + S(14), cardBattery.top + S(64), cardBattery.right - S(14), cardBattery.top + S(84) };
    DrawTextW(memDC, bDevName.c_str(), (int)bDevName.length(), &bDevRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    // MARK: Terminal Card
    RECT cardTerminal = { S(20), S(212), clientRc.right - S(20), clientRc.bottom - S(20) };
    DrawCard(memDC, cardTerminal);

    SelectObject(memDC, g_hFontSmall);
    SetTextColor(memDC, UI::ColorTextMuted);
    TextOutW(memDC, S(36), S(224), loc.Get(StringId::LogsTitle).c_str(), (int)loc.Get(StringId::LogsTitle).length());

    BitBlt(hdc, 0, 0, clientRc.right, clientRc.bottom, memDC, 0, 0, SRCCOPY);

    SelectObject(memDC, oldBmp);
    DeleteObject(memBitmap);
    DeleteDC(memDC);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg != 0 && msg == g_uTaskbarRestartMsg) {
        SetupTray(hwnd);
        UpdateTrayTooltip();
        return 0;
    }

    switch (msg) {
        case WM_SIZE: {
            if (wParam == SIZE_MINIMIZED) {
                TrimWorkingSet();
            }
            break;
        }

        case WM_CREATE: {
            g_uTaskbarRestartMsg = RegisterWindowMessageW(L"TaskbarCreated");
            g_dpi = SafeGetDpiForWindow(hwnd);
            if (g_dpi == 0) g_dpi = 96;

            g_hFontTitle = CreateFontW(-MulDiv(15, g_dpi, 72), 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
            g_hFontBody  = CreateFontW(-MulDiv(11, g_dpi, 72), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
            g_hFontSmall = CreateFontW(-MulDiv(9, g_dpi, 72), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
            g_hFontMono  = CreateFontW(-MulDiv(10, g_dpi, 72), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, FIXED_PITCH, L"Cascadia Mono, Consolas");

            g_hBrWindowBg = CreateSolidBrush(UI::ColorWindowBg);
            g_hBrCardBg   = CreateSolidBrush(UI::ColorCardBg);
            g_hBrEditBg   = CreateSolidBrush(UI::ColorCardBg);

            RECT rc;
            GetClientRect(hwnd, &rc);
            auto& loc = Localization::Instance();

            // MARK: Controls (Main View)
            g_hBtnStart = CreateWindowW(L"BUTTON", L"Start Service", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_OWNERDRAW,
                S(20), S(164), S(140), S(34), hwnd, (HMENU)(INT_PTR)IDC_BTN_START, GetModuleHandleW(NULL), NULL);

            g_hBtnStop = CreateWindowW(L"BUTTON", L"Stop Service", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_OWNERDRAW | WS_DISABLED,
                S(168), S(164), S(140), S(34), hwnd, (HMENU)(INT_PTR)IDC_BTN_STOP, GetModuleHandleW(NULL), NULL);

            // MARK: Top-Right Controls
            g_hBtnLang = CreateWindowW(L"BUTTON", L"TR", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_OWNERDRAW,
                rc.right - S(154), S(16), S(38), S(26), hwnd, (HMENU)(INT_PTR)IDC_BTN_LANG, GetModuleHandleW(NULL), NULL);

            g_hBtnSettings = CreateWindowW(L"BUTTON", L"⚙", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_OWNERDRAW,
                rc.right - S(108), S(16), S(38), S(26), hwnd, (HMENU)(INT_PTR)IDC_BTN_SETTINGS, GetModuleHandleW(NULL), NULL);

            g_hBtnTray = CreateWindowW(L"BUTTON", L"_", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_OWNERDRAW,
                rc.right - S(62), S(16), S(38), S(26), hwnd, (HMENU)(INT_PTR)IDC_BTN_TRAY, GetModuleHandleW(NULL), NULL);

            g_hBtnClearLogs = CreateWindowW(L"BUTTON", L"Clear", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_OWNERDRAW,
                rc.right - S(82), S(218), S(54), S(22), hwnd, (HMENU)(INT_PTR)IDC_BTN_CLEAR_LOGS, GetModuleHandleW(NULL), NULL);

            // MARK: Terminal Logs
            int editTop = S(248);
            int editHeight = (rc.bottom - S(20)) - editTop - S(12);
            if (editHeight < S(140)) editHeight = S(140);
            g_hEditLogs = CreateWindowExW(0, L"EDIT", L"",
                WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
                S(32), editTop, rc.right - S(64), editHeight, hwnd, (HMENU)(INT_PTR)IDC_EDIT_LOGS, GetModuleHandleW(NULL), NULL);

            SendMessageW(g_hEditLogs, WM_SETFONT, (WPARAM)g_hFontMono, TRUE);
            SetWindowTheme(g_hEditLogs, L"DarkMode_Explorer", NULL);

            // MARK: Settings View Controls (Hidden by default)
            g_hChkStartWindows = CreateWindowW(L"BUTTON", loc.Get(StringId::StartWithWindows).c_str(),
                WS_TABSTOP | WS_CHILD | BS_AUTOCHECKBOX,
                S(40), S(102), S(450), S(22), hwnd, (HMENU)(INT_PTR)IDC_CHK_START_WINDOWS, GetModuleHandleW(NULL), NULL);
            SendMessageW(g_hChkStartWindows, WM_SETFONT, (WPARAM)g_hFontBody, TRUE);
            SendMessageW(g_hChkStartWindows, BM_SETCHECK, g_startWithWindows ? BST_CHECKED : BST_UNCHECKED, 0);
            SetWindowTheme(g_hChkStartWindows, L"DarkMode_Explorer", NULL);

            g_hChkMinimizeClose = CreateWindowW(L"BUTTON", loc.Get(StringId::MinimizeOnClose).c_str(),
                WS_TABSTOP | WS_CHILD | BS_AUTOCHECKBOX,
                S(40), S(134), S(450), S(22), hwnd, (HMENU)(INT_PTR)IDC_CHK_MINIMIZE_CLOSE, GetModuleHandleW(NULL), NULL);
            SendMessageW(g_hChkMinimizeClose, WM_SETFONT, (WPARAM)g_hFontBody, TRUE);
            SendMessageW(g_hChkMinimizeClose, BM_SETCHECK, g_minimizeOnClose ? BST_CHECKED : BST_UNCHECKED, 0);
            SetWindowTheme(g_hChkMinimizeClose, L"DarkMode_Explorer", NULL);

            g_hChkAutoStart = CreateWindowW(L"BUTTON", loc.Get(StringId::AutoStartService).c_str(),
                WS_TABSTOP | WS_CHILD | BS_AUTOCHECKBOX,
                S(40), S(166), S(450), S(22), hwnd, (HMENU)(INT_PTR)IDC_CHK_AUTO_START, GetModuleHandleW(NULL), NULL);
            SendMessageW(g_hChkAutoStart, WM_SETFONT, (WPARAM)g_hFontBody, TRUE);
            SendMessageW(g_hChkAutoStart, BM_SETCHECK, g_autoStartService ? BST_CHECKED : BST_UNCHECKED, 0);
            SetWindowTheme(g_hChkAutoStart, L"DarkMode_Explorer", NULL);

            g_hChkLowBattery = CreateWindowW(L"BUTTON", loc.Get(StringId::LowBatteryNotification).c_str(),
                WS_TABSTOP | WS_CHILD | BS_AUTOCHECKBOX,
                S(40), S(198), S(450), S(22), hwnd, (HMENU)(INT_PTR)IDC_CHK_LOW_BATTERY, GetModuleHandleW(NULL), NULL);
            SendMessageW(g_hChkLowBattery, WM_SETFONT, (WPARAM)g_hFontBody, TRUE);
            SendMessageW(g_hChkLowBattery, BM_SETCHECK, g_lowBatteryAlert ? BST_CHECKED : BST_UNCHECKED, 0);
            SetWindowTheme(g_hChkLowBattery, L"DarkMode_Explorer", NULL);

            g_hChkNintendoMode = CreateWindowW(L"BUTTON", loc.Get(StringId::NintendoMode).c_str(),
                WS_TABSTOP | WS_CHILD | BS_AUTOCHECKBOX,
                S(40), S(230), S(450), S(22), hwnd, (HMENU)(INT_PTR)IDC_CHK_NINTENDO_MODE, GetModuleHandleW(NULL), NULL);
            SendMessageW(g_hChkNintendoMode, WM_SETFONT, (WPARAM)g_hFontBody, TRUE);
            SendMessageW(g_hChkNintendoMode, BM_SETCHECK, g_nintendoMode ? BST_CHECKED : BST_UNCHECKED, 0);
            SetWindowTheme(g_hChkNintendoMode, L"DarkMode_Explorer", NULL);

            g_hBtnDeadzone = CreateWindowW(L"BUTTON", L"",
                WS_TABSTOP | WS_CHILD | BS_OWNERDRAW,
                S(40), S(272), S(280), S(32), hwnd, (HMENU)(INT_PTR)IDC_BTN_DEADZONE, GetModuleHandleW(NULL), NULL);

            g_hBtnSettingsBack = CreateWindowW(L"BUTTON", loc.Get(StringId::SettingsBack).c_str(),
                WS_TABSTOP | WS_CHILD | BS_OWNERDRAW,
                S(40), S(320), S(120), S(32), hwnd, (HMENU)(INT_PTR)IDC_BTN_SETTINGS_BACK, GetModuleHandleW(NULL), NULL);

            UpdateDeadzoneButtonText();
            SetupTray(hwnd);

            // MARK: Driver & First-Launch Check
            g_driverInstalled = IsViGEmBusInstalled();
            bool isFirstRun = false;
            HKEY hAppKey;
            if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\Ultimate2CFixer", 0, NULL, 0, KEY_READ | KEY_WRITE, NULL, &hAppKey, NULL) == ERROR_SUCCESS) {
                DWORD val = 0, size = sizeof(val);
                if (RegQueryValueExW(hAppKey, L"FirstRunDone", NULL, NULL, (LPBYTE)&val, &size) != ERROR_SUCCESS) {
                    isFirstRun = true;
                    val = 1;
                    RegSetValueExW(hAppKey, L"FirstRunDone", 0, REG_DWORD, (const BYTE*)&val, sizeof(val));
                }
                RegCloseKey(hAppKey);
            }

            UpdateUIStrings();
            AppendLogMessage(loc.Get(StringId::LogAppReady));
            if (isFirstRun && g_driverInstalled) {
                AppendLogMessage(loc.Get(StringId::DriverReadyFirstRun));
            } else if (!g_driverInstalled) {
                AppendLogMessage(loc.Get(StringId::DriverMissing));
            }
            return 0;
        }

        case WM_DRAWITEM: {
            LPDRAWITEMSTRUCT dis = (LPDRAWITEMSTRUCT)lParam;
            if (dis->CtlType == ODT_BUTTON) {
                bool isAccent = (dis->CtlID == IDC_BTN_START || dis->CtlID == IDC_BTN_SETTINGS_BACK);
                DrawModernButton(dis, isAccent);
                return TRUE;
            }
            break;
        }

        case WM_COMMAND: {
            int id = LOWORD(wParam);
            switch (id) {
                case IDC_BTN_SETTINGS:
                    SwitchView(!g_showSettings);
                    break;
                case IDC_BTN_SETTINGS_BACK:
                    SwitchView(false);
                    break;
                case IDC_CHK_START_WINDOWS:
                    g_startWithWindows = (SendMessageW(g_hChkStartWindows, BM_GETCHECK, 0, 0) == BST_CHECKED);
                    ApplyStartWithWindows(g_startWithWindows);
                    break;
                case IDC_CHK_MINIMIZE_CLOSE:
                    g_minimizeOnClose = (SendMessageW(g_hChkMinimizeClose, BM_GETCHECK, 0, 0) == BST_CHECKED);
                    break;
                case IDC_CHK_AUTO_START:
                    g_autoStartService = (SendMessageW(g_hChkAutoStart, BM_GETCHECK, 0, 0) == BST_CHECKED);
                    break;
                case IDC_CHK_LOW_BATTERY:
                    g_lowBatteryAlert = (SendMessageW(g_hChkLowBattery, BM_GETCHECK, 0, 0) == BST_CHECKED);
                    break;
                case IDC_CHK_NINTENDO_MODE:
                    g_nintendoMode = (SendMessageW(g_hChkNintendoMode, BM_GETCHECK, 0, 0) == BST_CHECKED);
                    if (g_remapper) g_remapper->SetNintendoMode(g_nintendoMode);
                    InvalidateRect(hwnd, NULL, TRUE);
                    break;
                case IDC_BTN_DEADZONE:
                    g_deadzoneLevel = (g_deadzoneLevel + 1) % 4;
                    UpdateDeadzoneButtonText();
                    if (g_remapper) g_remapper->SetDeadzone(kDeadzoneValues[g_deadzoneLevel]);
                    break;
                case IDC_BTN_START: StartServices(); break;
                case IDC_BTN_STOP:  StopServices(); break;
                case IDC_BTN_CLEAR_LOGS:
                    g_logLines.clear();
                    SetWindowTextW(g_hEditLogs, L"");
                    break;
                case IDC_BTN_LANG:
                    Localization::Instance().ToggleLanguage();
                    UpdateUIStrings();
                    break;
                case IDC_BTN_TRAY:
                    MinimizeToTray();
                    break;
                case IDM_TRAY_OPEN:
                    RestoreFromTray();
                    break;
                case IDM_TRAY_EXIT:
                    DestroyWindow(hwnd);
                    break;
            }
            return 0;
        }

        case WM_UPDATE_INPUT: {
            if (!g_showSettings) {
                int gap = S(12);
                RECT rcTelemetry = { S(260) + gap, S(56), S(260) + gap + S(240), S(152) };
                InvalidateRect(hwnd, &rcTelemetry, FALSE);
            }
            return 0;
        }

        case WM_UPDATE_LOG: {
            auto* pMsg = reinterpret_cast<std::wstring*>(wParam);
            if (pMsg) {
                AppendLogMessage(*pMsg);
                delete pMsg;
            }
            return 0;
        }

        case WM_UPDATE_STATUS: {
            g_currentStatus = static_cast<RemapperStatus>(wParam);
            auto* pName = reinterpret_cast<std::wstring*>(lParam);
            if (pName) {
                g_deviceName = *pName;
                delete pName;
            }
            if (g_currentStatus != RemapperStatus::Connected) {
                g_deviceName.clear();
                memset(&g_liveInput, 0, sizeof(g_liveInput));
            }
            UpdateTrayTooltip();
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        }

        case WM_UPDATE_BATTERY: {
            g_batteryLevel = static_cast<int>(wParam);
            auto* pName = reinterpret_cast<std::wstring*>(lParam);
            if (pName) {
                g_batteryDevice = *pName;
                delete pName;
            }
            if (g_batteryLevel < 0) {
                g_batteryDevice.clear();
            }

            // MARK: Low Battery Toast Notification
            if (g_batteryLevel > 0 && g_batteryLevel <= 15 && !g_batteryWarningSent && g_lowBatteryAlert) {
                g_batteryWarningSent = true;
                auto& loc = Localization::Instance();
                g_nid.uFlags |= NIF_INFO;
                wcsncpy_s(g_nid.szInfoTitle, loc.Get(StringId::LowBatteryAlertTitle).c_str(), _TRUNCATE);
                wchar_t buf[256];
                swprintf_s(buf, loc.Get(StringId::LowBatteryAlertMsg).c_str(), g_batteryLevel);
                wcsncpy_s(g_nid.szInfo, buf, _TRUNCATE);
                g_nid.dwInfoFlags = NIIF_WARNING;
                Shell_NotifyIconW(NIM_MODIFY, &g_nid);
                g_nid.uFlags &= ~NIF_INFO;
            } else if (g_batteryLevel > 20) {
                g_batteryWarningSent = false;
            }

            UpdateTrayTooltip();
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        }


        case WM_DRIVER_INSTALL_PROGRESS: {
            int pct = (int)wParam;
            auto& loc = Localization::Instance();
            wchar_t buf[128];
            swprintf_s(buf, loc.Get(StringId::DriverDownloading).c_str(), pct);
            SetWindowTextW(g_hBtnStart, buf);
            return 0;
        }

        case WM_DRIVER_INSTALL_DONE: {
            bool success = (wParam == 1);
            g_driverInstalling = false;
            g_driverInstalled = success;
            auto& loc = Localization::Instance();
            EnableWindow(g_hBtnStart, TRUE);
            UpdateUIStrings();
            if (success) {
                AppendLogMessage(loc.Get(StringId::DriverSuccess));
                if (g_autoStartService) {
                    StartServices();
                }
            } else {
                AppendLogMessage(loc.Get(StringId::DriverFailed));
            }
            return 0;
        }

        case WM_CTLCOLORSTATIC:
        case WM_CTLCOLOREDIT: {
            HDC hdcCtrl = (HDC)wParam;
            HWND hwndCtrl = (HWND)lParam;
            if (hwndCtrl == g_hChkMinimizeClose || hwndCtrl == g_hChkStartWindows || hwndCtrl == g_hChkAutoStart || hwndCtrl == g_hChkLowBattery || hwndCtrl == g_hChkNintendoMode) {
                SetTextColor(hdcCtrl, UI::ColorTextSecondary);
                SetBkColor(hdcCtrl, UI::ColorCardBg);
                return (LRESULT)g_hBrCardBg;
            }
            if (hwndCtrl == g_hEditLogs) {
                SetTextColor(hdcCtrl, UI::ColorTextSecondary);
                SetBkColor(hdcCtrl, UI::ColorCardBg);
                return (LRESULT)g_hBrEditBg;
            }
            break;
        }

        case WM_CLOSE: {
            if (g_minimizeOnClose) {
                MinimizeToTray();
                return 0;
            }
            DestroyWindow(hwnd);
            return 0;
        }

        case WM_TRAYICON: {
            UINT event = LOWORD(lParam);
            if (event == WM_LBUTTONUP || event == WM_LBUTTONDBLCLK || event == NIN_SELECT) {
                RestoreFromTray();
            } else if (event == WM_RBUTTONUP || event == WM_CONTEXTMENU) {
                ShowTrayMenu();
            }
            return 0;
        }

        case WM_ERASEBKGND:
            return 1;

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            PaintDashboard(hwnd, hdc);
            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_DESTROY: {
            StopServices();
            if (g_trayCreated) {
                Shell_NotifyIconW(NIM_DELETE, &g_nid);
            }
            DeleteObject(g_hFontTitle);
            DeleteObject(g_hFontBody);
            DeleteObject(g_hFontSmall);
            DeleteObject(g_hFontMono);
            DeleteObject(g_hBrWindowBg);
            DeleteObject(g_hBrCardBg);
            DeleteObject(g_hBrEditBg);
            PostQuitMessage(0);
            return 0;
        }
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

} // namespace

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow) {
    InitDpiAwareness();

    g_startWithWindows = CheckStartWithWindows();

    INITCOMMONCONTROLSEX icex = {};
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_STANDARD_CLASSES | ICC_PROGRESS_CLASS;
    InitCommonControlsEx(&icex);

    const wchar_t CLASS_NAME[] = L"Ultimate2CFixer_Class";

    int cxSm = GetSystemMetrics(SM_CXSMICON);
    int cySm = GetSystemMetrics(SM_CYSMICON);
    HICON hIconBig = (HICON)LoadImageW(hInstance, MAKEINTRESOURCEW(1), IMAGE_ICON, 0, 0, LR_DEFAULTSIZE | LR_SHARED);
    HICON hIconSm = (HICON)LoadImageW(hInstance, MAKEINTRESOURCEW(1), IMAGE_ICON, cxSm, cySm, LR_DEFAULTCOLOR | LR_SHARED);
    if (!hIconBig) hIconBig = LoadIconW(hInstance, MAKEINTRESOURCEW(1));
    if (!hIconSm) hIconSm = hIconBig;

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursorW(NULL, (LPCWSTR)IDC_ARROW);
    wc.hIcon = hIconBig;
    wc.hIconSm = hIconSm;
    wc.hbrBackground = CreateSolidBrush(UI::ColorWindowBg);

    RegisterClassExW(&wc);

    UINT dpi = SafeGetDpiForSystem();
    int winWidth = MulDiv(720, dpi, 96);
    int winHeight = MulDiv(540, dpi, 96);

    g_hWnd = CreateWindowExW(
        0,
        CLASS_NAME,
        L"Ultimate2CFixer",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, winWidth, winHeight,
        NULL, NULL, hInstance, NULL
    );

    if (!g_hWnd) {
        return 0;
    }

    SendMessageW(g_hWnd, WM_SETICON, ICON_BIG, (LPARAM)hIconBig);
    SendMessageW(g_hWnd, WM_SETICON, ICON_SMALL, (LPARAM)hIconSm);

    UI::EnableImmersiveDarkMode(g_hWnd);

    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    bool startMinimized = false;
    if (argv) {
        for (int i = 1; i < argc; ++i) {
            if (wcscmp(argv[i], L"--minimized") == 0 || wcscmp(argv[i], L"-m") == 0) {
                startMinimized = true;
                break;
            }
        }
        LocalFree(argv);
    }

    if (startMinimized) {
        nCmdShow = SW_HIDE;
    }

    ShowWindow(g_hWnd, nCmdShow);
    UpdateWindow(g_hWnd);

    if (startMinimized) {
        MinimizeToTray();
    }

    if (g_autoStartService) {
        StartServices();
    }

    MSG msg = {};
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return (int)msg.wParam;
}
