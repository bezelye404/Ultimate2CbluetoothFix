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
#include <sstream>
#include <iomanip>
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
        g_nid.hIcon = LoadIconW(NULL, IDI_APPLICATION);
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

    RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, 10, 10);

    SelectObject(hdc, hOldBr);
    SelectObject(hdc, hOldPen);
    DeleteObject(hPen);
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
    TextOutW(memDC, 20, 18, loc.Get(StringId::AppTitle).c_str(), (int)loc.Get(StringId::AppTitle).length());

    SelectObject(memDC, g_hFontSmall);
    SetTextColor(memDC, UI::ColorTextMuted);
    TextOutW(memDC, 20, 42, loc.Get(StringId::ModeDesc).c_str(), (int)loc.Get(StringId::ModeDesc).length());

    // 2. Card 1: Controller Status
    RECT cardStatus = { 20, 72, 360, 160 };
    DrawCard(memDC, cardStatus);

    SelectObject(memDC, g_hFontSmall);
    SetTextColor(memDC, UI::ColorTextMuted);
    TextOutW(memDC, 36, 86, loc.Get(StringId::StatusTitle).c_str(), (int)loc.Get(StringId::StatusTitle).length());

    SelectObject(memDC, g_hFontTitle);
    SetTextColor(memDC, UI::ColorTextPrimary);
    std::wstring devName = g_deviceName.empty() ? loc.Get(StringId::NoDevice) : g_deviceName;
    TextOutW(memDC, 36, 106, devName.c_str(), (int)devName.length());

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

    Ellipse(memDC, 36, 136, 44, 144);

    SelectObject(memDC, hOldBrush);
    SelectObject(memDC, hOldPen);
    DeleteObject(hDotBrush);
    DeleteObject(hNoPen);

    SelectObject(memDC, g_hFontBody);
    SetTextColor(memDC, UI::ColorTextSecondary);
    TextOutW(memDC, 50, 133, statusText.c_str(), (int)statusText.length());

    // 3. Card 2: Battery Status
    RECT cardBattery = { 375, 72, 600, 160 };
    DrawCard(memDC, cardBattery);

    SelectObject(memDC, g_hFontSmall);
    SetTextColor(memDC, UI::ColorTextMuted);
    TextOutW(memDC, 390, 86, loc.Get(StringId::BatteryTitle).c_str(), (int)loc.Get(StringId::BatteryTitle).length());

    // Battery percentage
    std::wstring pctStr = (g_batteryLevel >= 0) ? (std::to_wstring(g_batteryLevel) + L"%") : L"--%";
    SelectObject(memDC, g_hFontTitle);
    SetTextColor(memDC, UI::ColorTextPrimary);
    RECT pctRc = { 390, 84, 584, 106 };
    DrawTextW(memDC, pctStr.c_str(), (int)pctStr.length(), &pctRc, DT_RIGHT | DT_SINGLELINE);

    // Minimal Horizontal Battery Bar
    RECT trackRc = { 390, 116, 584, 122 };
    HBRUSH hTrackBr = CreateSolidBrush(UI::ColorCardBorder);
    FillRect(memDC, &trackRc, hTrackBr);
    DeleteObject(hTrackBr);

    if (g_batteryLevel > 0) {
        int fillWidth = ((584 - 390) * min(g_batteryLevel, 100)) / 100;
        RECT fillRc = { 390, 116, 390 + fillWidth, 122 };
        COLORREF fillCol = (g_batteryLevel <= 20) ? UI::ColorStatusRed : (g_batteryLevel <= 50 ? UI::ColorStatusAmber : UI::ColorStatusGreen);
        HBRUSH hFillBr = CreateSolidBrush(fillCol);
        FillRect(memDC, &fillRc, hFillBr);
        DeleteObject(hFillBr);
    }

    SelectObject(memDC, g_hFontSmall);
    SetTextColor(memDC, UI::ColorTextMuted);
    std::wstring bDevName = g_batteryDevice.empty() ? loc.Get(StringId::NoDevice) : g_batteryDevice;
    TextOutW(memDC, 390, 133, bDevName.c_str(), (int)bDevName.length());

    // 4. Footer
    SelectObject(memDC, g_hFontSmall);
    SetTextColor(memDC, UI::ColorTextMuted);
    TextOutW(memDC, 20, clientRc.bottom - 24, loc.Get(StringId::Footer).c_str(), (int)loc.Get(StringId::Footer).length());

    BitBlt(hdc, 0, 0, clientRc.right, clientRc.bottom, memDC, 0, 0, SRCCOPY);

    SelectObject(memDC, oldBmp);
    DeleteObject(memBitmap);
    DeleteDC(memDC);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            g_hFontTitle = CreateFontW(19, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
            g_hFontBody  = CreateFontW(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
            g_hFontSmall = CreateFontW(12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
            g_hFontMono  = CreateFontW(13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, FIXED_PITCH, L"Consolas");

            g_hBrWindowBg = CreateSolidBrush(UI::ColorWindowBg);
            g_hBrCardBg   = CreateSolidBrush(UI::ColorCardBg);
            g_hBrEditBg   = CreateSolidBrush(RGB(22, 22, 26));

            // Start Service button
            g_hBtnStart = CreateWindowW(L"BUTTON", L"Start Service", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                20, 172, 130, 36, hwnd, (HMENU)IDC_BTN_START, GetModuleHandleW(NULL), NULL);

            // Stop Service button
            g_hBtnStop = CreateWindowW(L"BUTTON", L"Stop Service", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON | WS_DISABLED,
                160, 172, 130, 36, hwnd, (HMENU)IDC_BTN_STOP, GetModuleHandleW(NULL), NULL);

            // Language button
            g_hBtnLang = CreateWindowW(L"BUTTON", L"TR", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                510, 18, 42, 26, hwnd, (HMENU)IDC_BTN_LANG, GetModuleHandleW(NULL), NULL);

            // Tray button
            g_hBtnTray = CreateWindowW(L"BUTTON", L"_", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                558, 18, 42, 26, hwnd, (HMENU)IDC_BTN_TRAY, GetModuleHandleW(NULL), NULL);

            // Clear Logs button
            g_hBtnClearLogs = CreateWindowW(L"BUTTON", L"Clear", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                536, 218, 64, 22, hwnd, (HMENU)IDC_BTN_CLEAR_LOGS, GetModuleHandleW(NULL), NULL);

            // Terminal Logs Box
            g_hEditLogs = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
                20, 244, 580, 220, hwnd, (HMENU)IDC_EDIT_LOGS, GetModuleHandleW(NULL), NULL);

            SendMessageW(g_hEditLogs, WM_SETFONT, (WPARAM)g_hFontMono, TRUE);
            SendMessageW(g_hBtnStart, WM_SETFONT, (WPARAM)g_hFontBody, TRUE);
            SendMessageW(g_hBtnStop, WM_SETFONT, (WPARAM)g_hFontBody, TRUE);
            SendMessageW(g_hBtnLang, WM_SETFONT, (WPARAM)g_hFontSmall, TRUE);
            SendMessageW(g_hBtnTray, WM_SETFONT, (WPARAM)g_hFontSmall, TRUE);
            SendMessageW(g_hBtnClearLogs, WM_SETFONT, (WPARAM)g_hFontSmall, TRUE);

            SetupTray(hwnd);
            UpdateUIStrings();
            AppendLogMessage(Localization::Instance().Get(StringId::LogAppReady));
            return 0;
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
                SetBkColor(hdcEdit, RGB(22, 22, 26));
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
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wc.hIcon = LoadIconW(hInstance, MAKEINTRESOURCEW(1));
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);

    RegisterClassExW(&wc);

    g_hWnd = CreateWindowExW(
        0,
        CLASS_NAME,
        L"8BitDo Ultimate 2C Fixer",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 636, 520,
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
