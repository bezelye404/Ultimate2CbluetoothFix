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
#include <string>
#include <vector>
#include <deque>
#include <memory>
#include <algorithm>
#include "UIStyles.h"
#include "Localization.h"
#include "Remapper.h"
#include "BatteryMonitor.h"

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(linker, "\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

using namespace BitDoFixer;

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
constexpr int WM_UPDATE_BATTERY     = WM_USER + 4;

constexpr int IDC_BTN_START         = 101;
constexpr int IDC_BTN_STOP          = 102;
constexpr int IDC_BTN_LANG          = 103;
constexpr int IDC_BTN_TRAY          = 104;
constexpr int IDC_BTN_CLEAR_LOGS    = 105;
constexpr int IDC_EDIT_LOGS         = 106;

constexpr int IDM_TRAY_OPEN         = 201;
constexpr int IDM_TRAY_EXIT         = 202;

constexpr size_t MAX_LOG_LINES      = 100;

HWND g_hWnd                         = nullptr;
HWND g_hBtnStart                    = nullptr;
HWND g_hBtnStop                     = nullptr;
HWND g_hBtnLang                     = nullptr;
HWND g_hBtnTray                     = nullptr;
HWND g_hBtnClearLogs                = nullptr;
HWND g_hEditLogs                    = nullptr;

HFONT g_hFontTitle                  = nullptr;
HFONT g_hFontBody                   = nullptr;
HFONT g_hFontSmall                  = nullptr;
HFONT g_hFontMono                   = nullptr;

HBRUSH g_hBrWindowBg                = nullptr;
HBRUSH g_hBrCardBg                  = nullptr;
HBRUSH g_hBrEditBg                  = nullptr;

NOTIFYICONDATAW g_nid               = {};
bool g_trayCreated                  = false;

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

void UpdateUIStrings() {
    auto& loc = Localization::Instance();
    SetWindowTextW(g_hWnd, loc.Get(StringId::AppTitle).c_str());
    SetWindowTextW(g_hBtnStart, loc.Get(StringId::StartBtn).c_str());
    SetWindowTextW(g_hBtnStop, loc.Get(StringId::StopBtn).c_str());
    SetWindowTextW(g_hBtnClearLogs, loc.Get(StringId::ClearBtn).c_str());
    SetWindowTextW(g_hBtnLang, loc.IsEnglish() ? L"TR" : L"EN");
    InvalidateRect(g_hWnd, NULL, TRUE);
}

void StartServices() {
    if (g_remapper && g_remapper->IsRunning()) return;

    EnableWindow(g_hBtnStart, FALSE);
    EnableWindow(g_hBtnStop, TRUE);

    auto& loc = Localization::Instance();
    AppendLogMessage(loc.Get(StringId::LogServicesStarting));

    g_currentStatus = RemapperStatus::Searching;
    g_deviceName = loc.Get(StringId::StatusSearching);
    InvalidateRect(g_hWnd, NULL, FALSE);

    g_remapper = std::make_unique<Remapper>();
    g_remapper->Start(
        g_hWnd,
        [](const std::wstring& msg) {
            auto* pMsg = new std::wstring(msg);
            PostMessageW(g_hWnd, WM_UPDATE_LOG, (WPARAM)pMsg, 0);
        },
        [](RemapperStatus status, const std::wstring& devName) {
            auto* pName = new std::wstring(devName);
            PostMessageW(g_hWnd, WM_UPDATE_STATUS, (WPARAM)status, (LPARAM)pName);
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
    g_nid.cbSize = sizeof(NOTIFYICONDATAW);
    g_nid.hWnd = hwnd;
    g_nid.uID = 1001;
    g_nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    g_nid.uCallbackMessage = WM_TRAYICON;
    g_nid.hIcon = LoadIconW(GetModuleHandleW(NULL), MAKEINTRESOURCEW(1));
    if (!g_nid.hIcon) {
        g_nid.hIcon = LoadIconW(NULL, (LPCWSTR)IDI_APPLICATION);
    }
    wcscpy_s(g_nid.szTip, L"8BitDo Ultimate 2C Fixer");
    g_trayCreated = Shell_NotifyIconW(NIM_ADD, &g_nid);
}

void MinimizeToTray() {
    ShowWindow(g_hWnd, SW_HIDE);
    TrimWorkingSet();
}

void RestoreFromTray() {
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

    COLORREF bg = isAccent ? UI::ColorButtonAccent : UI::ColorButtonBg;
    COLORREF border = isAccent ? UI::ColorButtonAccent : UI::ColorCardBorder;
    COLORREF text = isAccent ? UI::ColorButtonAccentText : UI::ColorTextPrimary;

    if (isDisabled) {
        bg = RGB(24, 24, 28);
        border = RGB(35, 35, 42);
        text = UI::ColorTextMuted;
    } else if (isPressed) {
        bg = isAccent ? RGB(210, 210, 214) : RGB(30, 30, 36);
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

    // Button text
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

    // 1. Header Title & Subtitle
    SelectObject(memDC, g_hFontTitle);
    SetTextColor(memDC, UI::ColorTextPrimary);
    TextOutW(memDC, S(20), S(16), loc.Get(StringId::AppTitle).c_str(), (int)loc.Get(StringId::AppTitle).length());

    SelectObject(memDC, g_hFontSmall);
    SetTextColor(memDC, UI::ColorTextMuted);
    TextOutW(memDC, S(20), S(42), loc.Get(StringId::ModeDesc).c_str(), (int)loc.Get(StringId::ModeDesc).length());

    // 2. Card 1: Controller Status
    RECT cardStatus = { S(20), S(68), clientRc.right - S(230), S(156) };
    DrawCard(memDC, cardStatus);

    SelectObject(memDC, g_hFontSmall);
    SetTextColor(memDC, UI::ColorTextMuted);
    TextOutW(memDC, S(36), S(82), loc.Get(StringId::StatusTitle).c_str(), (int)loc.Get(StringId::StatusTitle).length());

    SelectObject(memDC, g_hFontTitle);
    SetTextColor(memDC, UI::ColorTextPrimary);
    std::wstring devName = g_deviceName.empty() ? loc.Get(StringId::NoDevice) : g_deviceName;
    TextOutW(memDC, S(36), S(102), devName.c_str(), (int)devName.length());

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

    HBRUSH hDotBrush = CreateSolidBrush(dotColor);
    HPEN hNoPen = CreatePen(PS_NULL, 0, 0);
    HBRUSH hOldBrush = (HBRUSH)SelectObject(memDC, hDotBrush);
    HPEN hOldPen = (HPEN)SelectObject(memDC, hNoPen);

    Ellipse(memDC, S(36), S(132), S(44), S(140));

    SelectObject(memDC, hOldBrush);
    SelectObject(memDC, hOldPen);
    DeleteObject(hDotBrush);
    DeleteObject(hNoPen);

    SelectObject(memDC, g_hFontBody);
    SetTextColor(memDC, UI::ColorTextSecondary);
    TextOutW(memDC, S(50), S(128), statusText.c_str(), (int)statusText.length());

    // 3. Card 2: Battery Status
    RECT cardBattery = { clientRc.right - S(216), S(68), clientRc.right - S(20), S(156) };
    DrawCard(memDC, cardBattery);

    SelectObject(memDC, g_hFontSmall);
    SetTextColor(memDC, UI::ColorTextMuted);
    TextOutW(memDC, cardBattery.left + S(16), S(82), loc.Get(StringId::BatteryTitle).c_str(), (int)loc.Get(StringId::BatteryTitle).length());

    // Battery percentage
    std::wstring pctStr = (g_batteryLevel >= 0) ? (std::to_wstring(g_batteryLevel) + L"%") : L"--%";
    SelectObject(memDC, g_hFontTitle);
    SetTextColor(memDC, UI::ColorTextPrimary);
    RECT pctRc = { cardBattery.left + S(16), S(80), cardBattery.right - S(16), S(102) };
    DrawTextW(memDC, pctStr.c_str(), (int)pctStr.length(), &pctRc, DT_RIGHT | DT_SINGLELINE);

    // MARK: Battery Bar
    RECT trackRc = { cardBattery.left + S(16), S(110), cardBattery.right - S(16), S(116) };
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
    std::wstring bDevName = g_batteryDevice.empty() ? loc.Get(StringId::NoDevice) : g_batteryDevice;
    TextOutW(memDC, cardBattery.left + S(16), S(128), bDevName.c_str(), (int)bDevName.length());

    // Card for logs/terminal background
    RECT cardTerminal = { S(20), S(236), clientRc.right - S(20), clientRc.bottom - S(32) };
    DrawCard(memDC, cardTerminal);

    SelectObject(memDC, g_hFontSmall);
    SetTextColor(memDC, UI::ColorTextMuted);
    TextOutW(memDC, S(36), S(244), loc.Get(StringId::LogsTitle).c_str(), (int)loc.Get(StringId::LogsTitle).length());

    // 4. Footer
    SelectObject(memDC, g_hFontSmall);
    SetTextColor(memDC, UI::ColorTextMuted);
    TextOutW(memDC, S(22), clientRc.bottom - S(22), loc.Get(StringId::Footer).c_str(), (int)loc.Get(StringId::Footer).length());

    BitBlt(hdc, 0, 0, clientRc.right, clientRc.bottom, memDC, 0, 0, SRCCOPY);

    SelectObject(memDC, oldBmp);
    DeleteObject(memBitmap);
    DeleteDC(memDC);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
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

            // MARK: Controls
            g_hBtnStart = CreateWindowW(L"BUTTON", L"Start Service", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_OWNERDRAW,
                S(20), S(170), S(140), S(36), hwnd, (HMENU)(INT_PTR)IDC_BTN_START, GetModuleHandleW(NULL), NULL);

            g_hBtnStop = CreateWindowW(L"BUTTON", L"Stop Service", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_OWNERDRAW | WS_DISABLED,
                S(170), S(170), S(140), S(36), hwnd, (HMENU)(INT_PTR)IDC_BTN_STOP, GetModuleHandleW(NULL), NULL);

            g_hBtnLang = CreateWindowW(L"BUTTON", L"TR", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_OWNERDRAW,
                rc.right - S(108), S(16), S(38), S(26), hwnd, (HMENU)(INT_PTR)IDC_BTN_LANG, GetModuleHandleW(NULL), NULL);

            g_hBtnTray = CreateWindowW(L"BUTTON", L"_", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_OWNERDRAW,
                rc.right - S(62), S(16), S(38), S(26), hwnd, (HMENU)(INT_PTR)IDC_BTN_TRAY, GetModuleHandleW(NULL), NULL);

            g_hBtnClearLogs = CreateWindowW(L"BUTTON", L"Clear", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_OWNERDRAW,
                rc.right - S(82), S(240), S(54), S(22), hwnd, (HMENU)(INT_PTR)IDC_BTN_CLEAR_LOGS, GetModuleHandleW(NULL), NULL);

            // MARK: Terminal Logs
            int editHeight = rc.bottom - S(320);
            if (editHeight < S(150)) editHeight = S(150);
            g_hEditLogs = CreateWindowExW(0, L"EDIT", L"",
                WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
                S(32), S(268), rc.right - S(64), editHeight, hwnd, (HMENU)(INT_PTR)IDC_EDIT_LOGS, GetModuleHandleW(NULL), NULL);

            SendMessageW(g_hEditLogs, WM_SETFONT, (WPARAM)g_hFontMono, TRUE);

            SetupTray(hwnd);
            UpdateUIStrings();
            AppendLogMessage(Localization::Instance().Get(StringId::LogAppReady));
            return 0;
        }

        case WM_DRAWITEM: {
            LPDRAWITEMSTRUCT dis = (LPDRAWITEMSTRUCT)lParam;
            if (dis->CtlType == ODT_BUTTON) {
                bool isAccent = (dis->CtlID == IDC_BTN_START);
                DrawModernButton(dis, isAccent);
                return TRUE;
            }
            break;
        }

        case WM_COMMAND: {
            int id = LOWORD(wParam);
            switch (id) {
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
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        }

        case WM_CTLCOLORSTATIC:
        case WM_CTLCOLOREDIT: {
            HDC hdcEdit = (HDC)wParam;
            HWND hwndCtrl = (HWND)lParam;
            if (hwndCtrl == g_hEditLogs) {
                SetTextColor(hdcEdit, UI::ColorTextSecondary);
                SetBkColor(hdcEdit, UI::ColorCardBg);
                return (LRESULT)g_hBrEditBg;
            }
            break;
        }

        case WM_TRAYICON: {
            if (lParam == WM_LBUTTONDBLCLK || lParam == WM_LBUTTONUP) {
                RestoreFromTray();
            } else if (lParam == WM_RBUTTONUP) {
                ShowTrayMenu();
            }
            return 0;
        }

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
    // 1. Enable Per-Monitor DPI Awareness V2 (Stops Windows from bitmap-stretching and blurring the UI)
    InitDpiAwareness();

    INITCOMMONCONTROLSEX icex = {};
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_STANDARD_CLASSES | ICC_PROGRESS_CLASS;
    InitCommonControlsEx(&icex);

    const wchar_t CLASS_NAME[] = L"BitDoFixer_Native_Class";

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursorW(NULL, (LPCWSTR)IDC_ARROW);
    wc.hIcon = LoadIconW(hInstance, MAKEINTRESOURCEW(1));
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);

    RegisterClassExW(&wc);

    UINT dpi = SafeGetDpiForSystem();
    int winWidth = MulDiv(640, dpi, 96);
    int winHeight = MulDiv(510, dpi, 96);

    g_hWnd = CreateWindowExW(
        0,
        CLASS_NAME,
        L"8BitDo Ultimate 2C Fixer",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, winWidth, winHeight,
        NULL, NULL, hInstance, NULL
    );

    if (!g_hWnd) {
        return 0;
    }

    UI::EnableImmersiveDarkMode(g_hWnd);
    ShowWindow(g_hWnd, nCmdShow);
    UpdateWindow(g_hWnd);

    MSG msg = {};
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return (int)msg.wParam;
}
