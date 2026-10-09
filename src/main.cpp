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
#include <richedit.h>
#include <shellapi.h>
#include <uxtheme.h>
#include <string>
#include <vector>
#include <deque>
#include <memory>
#include <mutex>
#include <algorithm>
#include "UIStyles.h"
#include "Localization.h"
#include "Remapper.h"
#include "BatteryMonitor.h"
#include "DriverInstaller.h"
#include "DeviceHider.h"
#include "ControllerHiding.h"
#include "ControllerDisconnect.h"

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
constexpr int WM_HIDING_INSTALL_DONE    = WM_USER + 8;
constexpr int WM_HIDING_NOTICE          = WM_USER + 9;
constexpr int WM_HIDING_STARTUP_CHECK   = WM_USER + 10;

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
constexpr int IDC_CHK_HAIR_TRIGGER  = 115;
constexpr int IDC_BTN_POLLING_RATE  = 116;
constexpr int IDC_BTN_CURVE         = 117;
constexpr int IDC_CHK_DISCONNECT    = 118;

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
HWND g_hChkHairTrigger              = nullptr;
HWND g_hChkDisconnect               = nullptr;
HWND g_hBtnDeadzone                 = nullptr;
HWND g_hBtnPollingRate              = nullptr;
HWND g_hBtnCurve                    = nullptr;
HWND g_hBtnSettingsBack             = nullptr;

bool g_showSettings                 = false;
bool g_minimizeOnClose              = false;   // the X button closes the application; the _ button hides it to the tray
bool g_disconnectOnExit             = false;
bool g_sessionEnding                = false;
bool g_startWithWindows             = false;
bool g_autoStartService             = true;
bool g_lowBatteryAlert              = true;
bool g_nintendoMode                 = false;
bool g_hairTrigger                  = false;
bool g_hidingInstalling             = false;
bool g_hidingOffered                = false;
bool g_pendingHidHideOffer          = false;   // the tray notification about the missing driver is waiting for a click
bool g_driverInstalled              = false;
bool g_driverInstalling             = false;
int g_deadzoneLevel                 = 2; // 0=0%, 1=8%, 2=12%, 3=20%
const int kDeadzoneValues[]         = { 0, 2600, 4000, 6500 };
int g_pollingRateIndex              = 1; // 0=125Hz, 1=250Hz, 2=500Hz, 3=1000Hz
const int kPollingRates[]           = { 125, 250, 500, 1000 };
int g_responseCurve                 = 0; // 0=Linear, 1=Smooth, 2=Aggressive
bool g_batteryWarningSent           = false;
XUSB_REPORT g_liveInput             = {};
std::mutex g_liveInputLock;                        // the worker thread writes, the UI thread reads
UINT g_uShowWindowMsg               = 0;           // sent by a second instance to bring this window forward

HFONT g_hFontTitle                  = nullptr;
HFONT g_hFontBody                   = nullptr;
HFONT g_hFontSmall                  = nullptr;
HFONT g_hFontMono                   = nullptr;

HBRUSH g_hBrWindowBg                = nullptr;
HBRUSH g_hBrCardBg                  = nullptr;
HBRUSH g_hBrEditBg                  = nullptr;
HBRUSH g_hBrGreen                   = nullptr;
HBRUSH g_hBrAmber                   = nullptr;
HBRUSH g_hBrRed                     = nullptr;
HBRUSH g_hBrButtonNormal            = nullptr;
HBRUSH g_hBrStickBg                 = nullptr;
HBRUSH g_hBrBadgeBg                 = nullptr;
HBRUSH g_hBrCardBorder              = nullptr;

HPEN g_hPenCardBorder               = nullptr;
HPEN g_hPenGreen                    = nullptr;
HPEN g_hPenCross                    = nullptr;
HPEN g_hPenNull                     = nullptr;

NOTIFYICONDATAW g_nid               = {};
bool g_trayCreated                  = false;
UINT g_uTaskbarRestartMsg           = 0;

std::unique_ptr<Remapper> g_remapper;
std::unique_ptr<ControllerHiding> g_hiding;   // lives as long as the window
std::unique_ptr<BatteryMonitor> g_batteryMonitor;

struct LogLine {
    std::wstring text;
    LogLevel level;
};
std::deque<LogLine> g_logLines;
std::wstring g_deviceName;
RemapperStatus g_currentStatus      = RemapperStatus::Stopped;
int g_batteryLevel                  = -1;
std::wstring g_batteryDevice;

int g_dpi = 96;
int S(int val) { return MulDiv(val, g_dpi, 96); }

XUSB_REPORT GetLiveInput() {
    std::lock_guard<std::mutex> lock(g_liveInputLock);
    return g_liveInput;
}

void SetLiveInput(const XUSB_REPORT& report) {
    std::lock_guard<std::mutex> lock(g_liveInputLock);
    g_liveInput = report;
}

void TrimWorkingSet() {
    SetProcessWorkingSetSize(GetCurrentProcess(), (SIZE_T)-1, (SIZE_T)-1);
}

COLORREF LogColor(LogLevel level) {
    switch (level) {
        case LogLevel::Good:     return UI::ColorStatusGreen;
        case LogLevel::Bad:      return UI::ColorStatusRed;
        case LogLevel::Critical: return UI::ColorStatusAmber;
        default:                 return UI::ColorTextSecondary;
    }
}

void AppendLogMessage(const std::wstring& msg, LogLevel level = LogLevel::Info) {
    SYSTEMTIME st;
    GetLocalTime(&st);
    wchar_t timeBuf[32];
    swprintf_s(timeBuf, L"[%02d:%02d:%02d] ", st.wHour, st.wMinute, st.wSecond);

    g_logLines.push_back({ std::wstring(timeBuf) + msg, level });
    while (g_logLines.size() > MAX_LOG_LINES) {
        g_logLines.pop_front();
    }

    std::wstring allLogs;
    for (const auto& line : g_logLines) {
        allLogs += line.text + L"\r\n";
    }

    SendMessageW(g_hEditLogs, WM_SETREDRAW, FALSE, 0);
    SetWindowTextW(g_hEditLogs, allLogs.c_str());

    // Colour the important lines. The rich edit stores a line break as one character.
    LONG start = 0;
    for (const auto& line : g_logLines) {
        const LONG length = static_cast<LONG>(line.text.length());
        if (line.level != LogLevel::Info) {
            CHARRANGE range = { start, start + length };
            SendMessageW(g_hEditLogs, EM_EXSETSEL, 0, (LPARAM)&range);
            CHARFORMAT2W format = {};
            format.cbSize = sizeof(format);
            format.dwMask = CFM_COLOR;
            format.crTextColor = LogColor(line.level);
            SendMessageW(g_hEditLogs, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&format);
        }
        start += length + 1;
    }

    const LONG endPos = GetWindowTextLengthW(g_hEditLogs);
    SendMessageW(g_hEditLogs, EM_SETSEL, endPos, endPos);
    SendMessageW(g_hEditLogs, WM_SETREDRAW, TRUE, 0);
    SendMessageW(g_hEditLogs, EM_SCROLLCARET, 0, 0);
    InvalidateRect(g_hEditLogs, NULL, TRUE);
}

void AppendLogId(StringId id) {
    AppendLogMessage(Localization::Instance().Get(id), LevelOf(id));
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

void LoadUserSettings() {
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Ultimate2CFixer\\Settings", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        DWORD val = 0, size = sizeof(val);
        // Settings written before version 2 stored the old default (minimize to tray on close) even when the user
        // never chose it, so that stored value is not trusted: the X button closes the application by default.
        DWORD settingsVersion = 0;
        if (RegQueryValueExW(hKey, L"SettingsVersion", NULL, NULL, (LPBYTE)&settingsVersion, &size) != ERROR_SUCCESS) {
            settingsVersion = 0;
        }
        size = sizeof(val);
        if (settingsVersion >= 2 && RegQueryValueExW(hKey, L"MinimizeOnClose", NULL, NULL, (LPBYTE)&val, &size) == ERROR_SUCCESS) {
            g_minimizeOnClose = (val != 0);
        }
        size = sizeof(val);
        if (RegQueryValueExW(hKey, L"DisconnectOnExit", NULL, NULL, (LPBYTE)&val, &size) == ERROR_SUCCESS) {
            g_disconnectOnExit = (val != 0);
        }
        size = sizeof(val);
        if (RegQueryValueExW(hKey, L"AutoStart", NULL, NULL, (LPBYTE)&val, &size) == ERROR_SUCCESS) {
            g_autoStartService = (val != 0);
        }
        size = sizeof(val);
        if (RegQueryValueExW(hKey, L"LowBatteryAlert", NULL, NULL, (LPBYTE)&val, &size) == ERROR_SUCCESS) {
            g_lowBatteryAlert = (val != 0);
        }
        size = sizeof(val);
        if (RegQueryValueExW(hKey, L"NintendoMode", NULL, NULL, (LPBYTE)&val, &size) == ERROR_SUCCESS) {
            g_nintendoMode = (val != 0);
        }
        size = sizeof(val);
        if (RegQueryValueExW(hKey, L"HairTrigger", NULL, NULL, (LPBYTE)&val, &size) == ERROR_SUCCESS) {
            g_hairTrigger = (val != 0);
        }
        size = sizeof(val);
        if (RegQueryValueExW(hKey, L"Deadzone", NULL, NULL, (LPBYTE)&val, &size) == ERROR_SUCCESS) {
            if (val <= 3) g_deadzoneLevel = static_cast<int>(val);
        }
        size = sizeof(val);
        if (RegQueryValueExW(hKey, L"PollingRate", NULL, NULL, (LPBYTE)&val, &size) == ERROR_SUCCESS) {
            if (val <= 3) g_pollingRateIndex = static_cast<int>(val);
        }
        size = sizeof(val);
        if (RegQueryValueExW(hKey, L"ResponseCurve", NULL, NULL, (LPBYTE)&val, &size) == ERROR_SUCCESS) {
            if (val <= 2) g_responseCurve = static_cast<int>(val);
        }
        size = sizeof(val);
        if (RegQueryValueExW(hKey, L"Language", NULL, NULL, (LPBYTE)&val, &size) == ERROR_SUCCESS) {
            // Stored values: 0 = Turkish, 1 = English (kept for older settings), 2 = Spanish.
            if (val == 0) Localization::Instance().SetLanguage(Language::Turkish);
            else if (val == 1) Localization::Instance().SetLanguage(Language::English);
            else if (val == 2) Localization::Instance().SetLanguage(Language::Spanish);
        }
        RegCloseKey(hKey);
    }
}

void SaveUserSettings() {
    HKEY hKey;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\Ultimate2CFixer\\Settings", 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
        DWORD val = 2;
        RegSetValueExW(hKey, L"SettingsVersion", 0, REG_DWORD, (const BYTE*)&val, sizeof(val));
        val = g_minimizeOnClose ? 1 : 0;
        RegSetValueExW(hKey, L"MinimizeOnClose", 0, REG_DWORD, (const BYTE*)&val, sizeof(val));
        val = g_disconnectOnExit ? 1 : 0;
        RegSetValueExW(hKey, L"DisconnectOnExit", 0, REG_DWORD, (const BYTE*)&val, sizeof(val));
        val = g_autoStartService ? 1 : 0;
        RegSetValueExW(hKey, L"AutoStart", 0, REG_DWORD, (const BYTE*)&val, sizeof(val));
        val = g_lowBatteryAlert ? 1 : 0;
        RegSetValueExW(hKey, L"LowBatteryAlert", 0, REG_DWORD, (const BYTE*)&val, sizeof(val));
        val = g_nintendoMode ? 1 : 0;
        RegSetValueExW(hKey, L"NintendoMode", 0, REG_DWORD, (const BYTE*)&val, sizeof(val));
        val = g_hairTrigger ? 1 : 0;
        RegSetValueExW(hKey, L"HairTrigger", 0, REG_DWORD, (const BYTE*)&val, sizeof(val));
        val = static_cast<DWORD>(g_deadzoneLevel);
        RegSetValueExW(hKey, L"Deadzone", 0, REG_DWORD, (const BYTE*)&val, sizeof(val));
        val = static_cast<DWORD>(g_pollingRateIndex);
        RegSetValueExW(hKey, L"PollingRate", 0, REG_DWORD, (const BYTE*)&val, sizeof(val));
        val = static_cast<DWORD>(g_responseCurve);
        RegSetValueExW(hKey, L"ResponseCurve", 0, REG_DWORD, (const BYTE*)&val, sizeof(val));
        switch (Localization::Instance().Current()) {
            case Language::Turkish: val = 0; break;
            case Language::Spanish: val = 2; break;
            default: val = 1; break;
        }
        RegSetValueExW(hKey, L"Language", 0, REG_DWORD, (const BYTE*)&val, sizeof(val));
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

void UpdatePollingRateButtonText() {
    auto& loc = Localization::Instance();
    std::wstring text = loc.Get(StringId::PollingRateLabel) + L": " + std::to_wstring(kPollingRates[g_pollingRateIndex]) + L" Hz";
    SetWindowTextW(g_hBtnPollingRate, text.c_str());
}

void UpdateCurveButtonText() {
    auto& loc = Localization::Instance();
    std::wstring text = loc.Get(StringId::CurveLabel) + L": ";
    switch (g_responseCurve) {
        case 0: text += loc.Get(StringId::CurveLinear); break;
        case 1: text += loc.Get(StringId::CurveSmooth); break;
        case 2: text += loc.Get(StringId::CurveAggressive); break;
    }
    SetWindowTextW(g_hBtnCurve, text.c_str());
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
    ShowWindow(g_hChkHairTrigger, showSet);
    ShowWindow(g_hChkDisconnect, showSet);
    ShowWindow(g_hBtnDeadzone, showSet);
    ShowWindow(g_hBtnPollingRate, showSet);
    ShowWindow(g_hBtnCurve, showSet);
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
    SetWindowTextW(g_hBtnLang, loc.Code());
    SetWindowTextW(g_hChkStartWindows, loc.Get(StringId::StartWithWindows).c_str());
    SetWindowTextW(g_hChkMinimizeClose, loc.Get(StringId::MinimizeOnClose).c_str());
    SetWindowTextW(g_hChkAutoStart, loc.Get(StringId::AutoStartService).c_str());
    SetWindowTextW(g_hChkLowBattery, loc.Get(StringId::LowBatteryNotification).c_str());
    SetWindowTextW(g_hChkNintendoMode, loc.Get(StringId::NintendoMode).c_str());
    SetWindowTextW(g_hChkHairTrigger, loc.Get(StringId::HairTrigger).c_str());
    SetWindowTextW(g_hChkDisconnect, loc.Get(StringId::DisconnectOnExit).c_str());
    SetWindowTextW(g_hBtnSettingsBack, loc.Get(StringId::SettingsBack).c_str());
    UpdateDeadzoneButtonText();
    UpdatePollingRateButtonText();
    UpdateCurveButtonText();
    UpdateTrayTooltip();
    InvalidateRect(g_hWnd, NULL, TRUE);
}

void TriggerDriverInstall() {
    if (g_driverInstalling) return;
    g_driverInstalling = true;
    EnableWindow(g_hBtnStart, FALSE);
    AppendLogId(StringId::DriverInstalling);
    StartViGEmBusInstall(
        g_hWnd,
        [](int pct, const std::wstring&) {
            PostMessageW(g_hWnd, WM_DRIVER_INSTALL_PROGRESS, (WPARAM)pct, 0);
        },
        [](bool success, const std::wstring& message) {
            int code = success ? 1 : (message == kDriverNotVerified ? 2 : 0);
            PostMessageW(g_hWnd, WM_DRIVER_INSTALL_DONE, (WPARAM)code, 0);
        }
    );
}

// Offers the optional hiding driver when hiding is wanted but the driver is missing.
void OfferHidingDriverInstall(bool force) {
    if (g_hidingInstalling || DeviceHider::IsAvailable()) return;
    if (!force && g_hidingOffered) return;
    g_hidingOffered = true;

    auto& loc = Localization::Instance();
    int answer = MessageBoxW(g_hWnd, loc.Get(StringId::HideInstallPrompt).c_str(),
                             loc.Get(StringId::HideInstallTitle).c_str(), MB_YESNO | MB_ICONQUESTION);
    if (answer != IDYES) return;

    g_hidingInstalling = true;
    AppendLogId(StringId::LogHideInstalling);
    StartHidHideInstall(g_hWnd, [](HidHideInstallResult result) {
        PostMessageW(g_hWnd, WM_HIDING_INSTALL_DONE, (WPARAM)result, 0);
    });
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
    AppendLogId(StringId::LogServicesStarting);

    g_currentStatus = RemapperStatus::Searching;
    g_deviceName = loc.Get(StringId::StatusSearching);
    InvalidateRect(g_hWnd, NULL, FALSE);

    g_remapper = std::make_unique<Remapper>();
    g_remapper->SetDeadzone(kDeadzoneValues[g_deadzoneLevel]);
    g_remapper->SetNintendoMode(g_nintendoMode);
    g_remapper->SetHairTrigger(g_hairTrigger);
    g_remapper->SetPollingRate(kPollingRates[g_pollingRateIndex]);
    g_remapper->SetResponseCurve(g_responseCurve);
    g_remapper->SetHiding(g_hiding.get());
    const bool started = g_remapper->Start(
        g_hWnd,
        [](const std::wstring& msg, LogLevel level) {
            auto* pMsg = new std::wstring(msg);
            PostMessageW(g_hWnd, WM_UPDATE_LOG, (WPARAM)pMsg, (LPARAM)level);
        },
        [](RemapperStatus status, const std::wstring& devName) {
            auto* pName = new std::wstring(devName);
            PostMessageW(g_hWnd, WM_UPDATE_STATUS, (WPARAM)status, (LPARAM)pName);
        },
        [](const XUSB_REPORT& report) {
            SetLiveInput(report);
            PostMessageW(g_hWnd, WM_UPDATE_INPUT, 0, 0);
        }
    );

    if (!started) {
        // The controller service could not start (the reason is in the log). Leave the window in a state the
        // user can act on instead of showing "Searching" forever.
        g_remapper.reset();
        g_currentStatus = RemapperStatus::Stopped;
        g_deviceName = loc.Get(StringId::NoDevice);
        EnableWindow(g_hBtnStart, TRUE);
        EnableWindow(g_hBtnStop, FALSE);
        if (!IsViGEmBusInstalled()) {
            g_driverInstalled = false;   // the driver went missing: offer the installer again
            UpdateUIStrings();
        }
        InvalidateRect(g_hWnd, NULL, FALSE);
        return;
    }

    g_batteryMonitor = std::make_unique<BatteryMonitor>();
    g_batteryMonitor->Start(
        [](const std::wstring& msg, LogLevel level) {
            auto* pMsg = new std::wstring(msg);
            PostMessageW(g_hWnd, WM_UPDATE_LOG, (WPARAM)pMsg, (LPARAM)level);
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
    AppendLogId(StringId::LogServicesStopping);

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

void ShowTrayBalloon(const std::wstring& text) {
    if (!g_trayCreated) return;
    g_nid.uFlags |= NIF_INFO;
    wcsncpy_s(g_nid.szInfoTitle, Localization::Instance().Get(StringId::AppTitle).c_str(), _TRUNCATE);
    wcsncpy_s(g_nid.szInfo, text.c_str(), _TRUNCATE);
    g_nid.dwInfoFlags = NIIF_INFO;
    Shell_NotifyIconW(NIM_MODIFY, &g_nid);
    g_nid.uFlags &= ~NIF_INFO;
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
    const XUSB_REPORT live = GetLiveInput();

    if (g_showSettings) {
        // MARK: Settings View Card
        RECT cardSettings = { S(24), S(56), clientRc.right - S(24), clientRc.bottom - S(20) };
        DrawCard(memDC, cardSettings);

        SelectObject(memDC, g_hFontTitle);
        SetTextColor(memDC, UI::ColorTextPrimary);
        TextOutW(memDC, S(44), S(72), loc.Get(StringId::SettingsTitle).c_str(), (int)loc.Get(StringId::SettingsTitle).length());

        BitBlt(hdc, 0, 0, clientRc.right, clientRc.bottom, memDC, 0, 0, SRCCOPY);
        SelectObject(memDC, oldBmp);
        DeleteObject(memBitmap);
        DeleteDC(memDC);
        return;
    }

    // MARK: Header
    SelectObject(memDC, g_hFontTitle);
    SetTextColor(memDC, UI::ColorTextPrimary);
    TextOutW(memDC, S(24), S(18), loc.Get(StringId::AppTitle).c_str(), (int)loc.Get(StringId::AppTitle).length());

    int topCardTop = S(56);
    int topCardBottom = S(152);

    // MARK: Card 1 - Controller Status Card
    RECT cardStatus = { S(24), topCardTop, S(264), topCardBottom };
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
    RECT cardTelemetry = { S(278), topCardTop, S(558), topCardBottom };
    DrawCard(memDC, cardTelemetry);

    SelectObject(memDC, g_hFontSmall);
    SetTextColor(memDC, UI::ColorTextMuted);
    TextOutW(memDC, cardTelemetry.left + S(16), cardTelemetry.top + S(14), loc.Get(StringId::LiveInputTitle).c_str(), (int)loc.Get(StringId::LiveInputTitle).length());

    // Mode Badge Pill
    std::wstring badgeText = g_nintendoMode ? L"NINTENDO" : L"XBOX";
    RECT badgeRc = { cardTelemetry.right - S(80), cardTelemetry.top + S(12), cardTelemetry.right - S(14), cardTelemetry.top + S(26) };
    SelectObject(memDC, g_hBrBadgeBg);
    SelectObject(memDC, g_hPenCardBorder);
    RoundRect(memDC, badgeRc.left, badgeRc.top, badgeRc.right, badgeRc.bottom, S(4), S(4));

    SetTextColor(memDC, g_nintendoMode ? UI::ColorStatusAmber : UI::ColorStatusGreen);
    DrawTextW(memDC, badgeText.c_str(), (int)badgeText.length(), &badgeRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    // Live Polling Rate Readout
    if (g_remapper && g_currentStatus == RemapperStatus::Connected) {
        wchar_t hzBuf[64];
        if (g_remapper->IsInputIdle()) {
            wcsncpy_s(hzBuf, loc.Get(StringId::Idle).c_str(), _TRUNCATE);
        } else {
            swprintf_s(hzBuf, L"%d Hz \u2022 %.1f ms", g_remapper->GetLiveHz(), g_remapper->GetLiveMs());
        }
        SelectObject(memDC, g_hFontSmall);
        SetTextColor(memDC, UI::ColorTextMuted);
        TextOutW(memDC, cardTelemetry.left + S(16), cardTelemetry.top + S(28), hzBuf, (int)wcslen(hzBuf));
    }

    // Left Stick & Right Stick Boxes
    int visX = cardTelemetry.left + S(16);
    int visY = cardTelemetry.top + S(42);

    RECT lsBox = { visX, visY, visX + S(34), visY + S(34) };
    RECT rsBox = { visX + S(40), visY, visX + S(74), visY + S(34) };
    SelectObject(memDC, g_hBrStickBg);
    SelectObject(memDC, g_hPenCardBorder);
    RoundRect(memDC, lsBox.left, lsBox.top, lsBox.right, lsBox.bottom, S(4), S(4));
    RoundRect(memDC, rsBox.left, rsBox.top, rsBox.right, rsBox.bottom, S(4), S(4));

    // Stick crosshairs
    SelectObject(memDC, g_hPenCross);
    MoveToEx(memDC, lsBox.left + S(17), lsBox.top + S(6), NULL);
    LineTo(memDC, lsBox.left + S(17), lsBox.bottom - S(6));
    MoveToEx(memDC, lsBox.left + S(6), lsBox.top + S(17), NULL);
    LineTo(memDC, lsBox.right - S(6), lsBox.top + S(17));

    MoveToEx(memDC, rsBox.left + S(17), rsBox.top + S(6), NULL);
    LineTo(memDC, rsBox.left + S(17), rsBox.bottom - S(6));
    MoveToEx(memDC, rsBox.left + S(6), rsBox.top + S(17), NULL);
    LineTo(memDC, rsBox.right - S(6), rsBox.top + S(17));

    // Live Stick Dots
    int lsDotX = lsBox.left + S(17) + (live.sThumbLX * S(12)) / 32768;
    int lsDotY = lsBox.top + S(17) - (live.sThumbLY * S(12)) / 32768;
    int rsDotX = rsBox.left + S(17) + (live.sThumbRX * S(12)) / 32768;
    int rsDotY = rsBox.top + S(17) - (live.sThumbRY * S(12)) / 32768;

    SelectObject(memDC, g_hBrGreen);
    SelectObject(memDC, g_hPenNull);
    Ellipse(memDC, lsDotX - S(3), lsDotY - S(3), lsDotX + S(3), lsDotY + S(3));
    Ellipse(memDC, rsDotX - S(3), rsDotY - S(3), rsDotX + S(3), rsDotY + S(3));

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
        bool on = (live.wButtons & b.m) != 0;
        SelectObject(memDC, on ? g_hBrGreen : g_hBrButtonNormal);
        SelectObject(memDC, on ? g_hPenGreen : g_hPenCardBorder);
        RECT brc = { b.x, b.y, b.x + S(12), b.y + S(12) };
        RoundRect(memDC, brc.left, brc.top, brc.right, brc.bottom, S(3), S(3));
        SetTextColor(memDC, on ? RGB(10, 20, 15) : UI::ColorTextMuted);
        DrawTextW(memDC, b.lbl, 1, &brc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    // LB / RB Bumpers (Shoulder buttons)
    struct BumperDef { const wchar_t* lbl; USHORT m; RECT rc; };
    BumperDef bumpers[2] = {
        { L"LB", XUSB_GAMEPAD_LEFT_SHOULDER,  { visX + S(134), visY + S(8), visX + S(162), visY + S(26) } },
        { L"RB", XUSB_GAMEPAD_RIGHT_SHOULDER, { visX + S(168), visY + S(8), visX + S(196), visY + S(26) } }
    };

    for (const auto& bmp : bumpers) {
        bool on = (live.wButtons & bmp.m) != 0;
        SelectObject(memDC, on ? g_hBrGreen : g_hBrButtonNormal);
        SelectObject(memDC, on ? g_hPenGreen : g_hPenCardBorder);
        RoundRect(memDC, bmp.rc.left, bmp.rc.top, bmp.rc.right, bmp.rc.bottom, S(4), S(4));
        SetTextColor(memDC, on ? RGB(10, 20, 15) : UI::ColorTextMuted);
        DrawTextW(memDC, bmp.lbl, 2, const_cast<LPRECT>(&bmp.rc), DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    // LT / RT Analog Trigger Meters (Smooth Real-time Fill)
    int trigX = visX + S(210);
    RECT ltRc = { trigX, visY + S(2), trigX + S(9), visY + S(32) };
    RECT rtRc = { trigX + S(13), visY + S(2), trigX + S(22), visY + S(32) };
    FillRect(memDC, &ltRc, g_hBrStickBg);
    FillRect(memDC, &rtRc, g_hBrStickBg);

    if (live.bLeftTrigger > 0) {
        int fh = (live.bLeftTrigger * S(30)) / 255;
        RECT frc = { ltRc.left, ltRc.bottom - fh, ltRc.right, ltRc.bottom };
        FillRect(memDC, &frc, g_hBrGreen);
    }
    if (live.bRightTrigger > 0) {
        int fh = (live.bRightTrigger * S(30)) / 255;
        RECT frc = { rtRc.left, rtRc.bottom - fh, rtRc.right, rtRc.bottom };
        FillRect(memDC, &frc, g_hBrGreen);
    }

    // MARK: Card 3 - Battery Card
    RECT cardBattery = { S(572), topCardTop, clientRc.right - S(24), topCardBottom };
    DrawCard(memDC, cardBattery);

    SelectObject(memDC, g_hFontSmall);
    SetTextColor(memDC, UI::ColorTextMuted);
    TextOutW(memDC, cardBattery.left + S(16), cardBattery.top + S(14), loc.Get(StringId::BatteryTitle).c_str(), (int)loc.Get(StringId::BatteryTitle).length());

    std::wstring pctStr = (g_batteryLevel >= 0) ? (std::to_wstring(g_batteryLevel) + L"%") : L"--%";
    SelectObject(memDC, g_hFontTitle);
    SetTextColor(memDC, UI::ColorTextPrimary);
    RECT pctRc = { cardBattery.left + S(16), cardBattery.top + S(10), cardBattery.right - S(16), cardBattery.top + S(36) };
    DrawTextW(memDC, pctStr.c_str(), (int)pctStr.length(), &pctRc, DT_RIGHT | DT_SINGLELINE);

    // Battery Bar (Height 8px, clean and solid)
    RECT trackRc = { cardBattery.left + S(16), cardBattery.top + S(46), cardBattery.right - S(16), cardBattery.top + S(54) };
    FillRect(memDC, &trackRc, g_hBrCardBorder);

    if (g_batteryLevel > 0) {
        int trackWidth = trackRc.right - trackRc.left;
        int fillWidth = (trackWidth * (std::min)(g_batteryLevel, 100)) / 100;
        RECT fillRc = { trackRc.left, trackRc.top, trackRc.left + fillWidth, trackRc.bottom };
        HBRUSH hFillBr = (g_batteryLevel <= 20) ? g_hBrRed : (g_batteryLevel <= 50 ? g_hBrAmber : g_hBrGreen);
        FillRect(memDC, &fillRc, hFillBr);
    }

    // Clean, uncrowded status description
    SelectObject(memDC, g_hFontSmall);
    std::wstring bStatusStr;
    COLORREF bStatusCol = UI::ColorTextMuted;
    if (g_batteryLevel >= 0) {
        if (g_batteryLevel <= 20) {
            bStatusStr = loc.Get(StringId::BatteryLowStatus);
            bStatusCol = UI::ColorStatusRed;
        } else if (g_batteryLevel <= 50) {
            bStatusStr = loc.Get(StringId::BatteryModerateStatus);
            bStatusCol = UI::ColorTextSecondary;
        } else {
            bStatusStr = loc.Get(StringId::BatteryHealthyStatus);
            bStatusCol = UI::ColorTextSecondary;
        }
    } else {
        bStatusStr = loc.Get(StringId::NoDevice);
        bStatusCol = UI::ColorTextMuted;
    }
    SetTextColor(memDC, bStatusCol);
    RECT bDevRc = { cardBattery.left + S(16), cardBattery.top + S(64), cardBattery.right - S(16), cardBattery.top + S(84) };
    DrawTextW(memDC, bStatusStr.c_str(), (int)bStatusStr.length(), &bDevRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

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

    if (g_uShowWindowMsg != 0 && msg == g_uShowWindowMsg) {
        RestoreFromTray();
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
            g_hBrGreen    = CreateSolidBrush(UI::ColorStatusGreen);
            g_hBrAmber    = CreateSolidBrush(UI::ColorStatusAmber);
            g_hBrRed      = CreateSolidBrush(UI::ColorStatusRed);
            g_hBrButtonNormal = CreateSolidBrush(RGB(28, 28, 35));
            g_hBrStickBg  = CreateSolidBrush(RGB(24, 24, 30));
            g_hBrBadgeBg  = CreateSolidBrush(RGB(32, 32, 40));
            g_hBrCardBorder = CreateSolidBrush(UI::ColorCardBorder);

            g_hPenCardBorder = CreatePen(PS_SOLID, 1, UI::ColorCardBorder);
            g_hPenGreen      = CreatePen(PS_SOLID, 1, UI::ColorStatusGreen);
            g_hPenCross      = CreatePen(PS_SOLID, 1, RGB(40, 40, 50));
            g_hPenNull       = CreatePen(PS_NULL, 0, 0);

            RECT rc;
            GetClientRect(hwnd, &rc);
            auto& loc = Localization::Instance();

            // MARK: Controls (Main View)
            g_hBtnStart = CreateWindowW(L"BUTTON", L"Start Service", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_OWNERDRAW,
                S(24), S(164), S(150), S(34), hwnd, (HMENU)(INT_PTR)IDC_BTN_START, GetModuleHandleW(NULL), NULL);

            g_hBtnStop = CreateWindowW(L"BUTTON", L"Stop Service", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_OWNERDRAW | WS_DISABLED,
                S(184), S(164), S(150), S(34), hwnd, (HMENU)(INT_PTR)IDC_BTN_STOP, GetModuleHandleW(NULL), NULL);

            // MARK: Top-Right Controls
            g_hBtnLang = CreateWindowW(L"BUTTON", L"TR", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_OWNERDRAW,
                rc.right - S(160), S(16), S(38), S(26), hwnd, (HMENU)(INT_PTR)IDC_BTN_LANG, GetModuleHandleW(NULL), NULL);

            g_hBtnSettings = CreateWindowW(L"BUTTON", L"\u2699", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_OWNERDRAW,
                rc.right - S(112), S(16), S(38), S(26), hwnd, (HMENU)(INT_PTR)IDC_BTN_SETTINGS, GetModuleHandleW(NULL), NULL);

            g_hBtnTray = CreateWindowW(L"BUTTON", L"_", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_OWNERDRAW,
                rc.right - S(64), S(16), S(38), S(26), hwnd, (HMENU)(INT_PTR)IDC_BTN_TRAY, GetModuleHandleW(NULL), NULL);

            g_hBtnClearLogs = CreateWindowW(L"BUTTON", L"Clear", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_OWNERDRAW,
                rc.right - S(98), S(218), S(64), S(22), hwnd, (HMENU)(INT_PTR)IDC_BTN_CLEAR_LOGS, GetModuleHandleW(NULL), NULL);

            // MARK: Terminal Logs
            int editTop = S(248);
            int editHeight = (rc.bottom - S(20)) - editTop - S(12);
            if (editHeight < S(140)) editHeight = S(140);
            g_hEditLogs = CreateWindowExW(0, MSFTEDIT_CLASS, L"",
                WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
                S(38), editTop, rc.right - S(76), editHeight, hwnd, (HMENU)(INT_PTR)IDC_EDIT_LOGS, GetModuleHandleW(NULL), NULL);

            SendMessageW(g_hEditLogs, WM_SETFONT, (WPARAM)g_hFontMono, TRUE);
            SendMessageW(g_hEditLogs, EM_SETBKGNDCOLOR, 0, (LPARAM)UI::ColorCardBg);
            {
                CHARFORMAT2W normalText = {};
                normalText.cbSize = sizeof(normalText);
                normalText.dwMask = CFM_COLOR;
                normalText.crTextColor = UI::ColorTextSecondary;
                SendMessageW(g_hEditLogs, EM_SETCHARFORMAT, SCF_DEFAULT, (LPARAM)&normalText);
            }
            SetWindowTheme(g_hEditLogs, L"DarkMode_Explorer", NULL);

            // MARK: Settings View Controls (Hidden by default)
            g_hChkStartWindows = CreateWindowW(L"BUTTON", loc.Get(StringId::StartWithWindows).c_str(),
                WS_TABSTOP | WS_CHILD | BS_AUTOCHECKBOX,
                S(44), S(106), S(650), S(22), hwnd, (HMENU)(INT_PTR)IDC_CHK_START_WINDOWS, GetModuleHandleW(NULL), NULL);
            SendMessageW(g_hChkStartWindows, WM_SETFONT, (WPARAM)g_hFontBody, TRUE);
            SendMessageW(g_hChkStartWindows, BM_SETCHECK, g_startWithWindows ? BST_CHECKED : BST_UNCHECKED, 0);
            SetWindowTheme(g_hChkStartWindows, L"DarkMode_Explorer", NULL);

            g_hChkMinimizeClose = CreateWindowW(L"BUTTON", loc.Get(StringId::MinimizeOnClose).c_str(),
                WS_TABSTOP | WS_CHILD | BS_AUTOCHECKBOX,
                S(44), S(136), S(650), S(22), hwnd, (HMENU)(INT_PTR)IDC_CHK_MINIMIZE_CLOSE, GetModuleHandleW(NULL), NULL);
            SendMessageW(g_hChkMinimizeClose, WM_SETFONT, (WPARAM)g_hFontBody, TRUE);
            SendMessageW(g_hChkMinimizeClose, BM_SETCHECK, g_minimizeOnClose ? BST_CHECKED : BST_UNCHECKED, 0);
            SetWindowTheme(g_hChkMinimizeClose, L"DarkMode_Explorer", NULL);

            g_hChkAutoStart = CreateWindowW(L"BUTTON", loc.Get(StringId::AutoStartService).c_str(),
                WS_TABSTOP | WS_CHILD | BS_AUTOCHECKBOX,
                S(44), S(166), S(650), S(22), hwnd, (HMENU)(INT_PTR)IDC_CHK_AUTO_START, GetModuleHandleW(NULL), NULL);
            SendMessageW(g_hChkAutoStart, WM_SETFONT, (WPARAM)g_hFontBody, TRUE);
            SendMessageW(g_hChkAutoStart, BM_SETCHECK, g_autoStartService ? BST_CHECKED : BST_UNCHECKED, 0);
            SetWindowTheme(g_hChkAutoStart, L"DarkMode_Explorer", NULL);

            g_hChkLowBattery = CreateWindowW(L"BUTTON", loc.Get(StringId::LowBatteryNotification).c_str(),
                WS_TABSTOP | WS_CHILD | BS_AUTOCHECKBOX,
                S(44), S(196), S(650), S(22), hwnd, (HMENU)(INT_PTR)IDC_CHK_LOW_BATTERY, GetModuleHandleW(NULL), NULL);
            SendMessageW(g_hChkLowBattery, WM_SETFONT, (WPARAM)g_hFontBody, TRUE);
            SendMessageW(g_hChkLowBattery, BM_SETCHECK, g_lowBatteryAlert ? BST_CHECKED : BST_UNCHECKED, 0);
            SetWindowTheme(g_hChkLowBattery, L"DarkMode_Explorer", NULL);

            g_hChkNintendoMode = CreateWindowW(L"BUTTON", loc.Get(StringId::NintendoMode).c_str(),
                WS_TABSTOP | WS_CHILD | BS_AUTOCHECKBOX,
                S(44), S(226), S(650), S(22), hwnd, (HMENU)(INT_PTR)IDC_CHK_NINTENDO_MODE, GetModuleHandleW(NULL), NULL);
            SendMessageW(g_hChkNintendoMode, WM_SETFONT, (WPARAM)g_hFontBody, TRUE);
            SendMessageW(g_hChkNintendoMode, BM_SETCHECK, g_nintendoMode ? BST_CHECKED : BST_UNCHECKED, 0);
            SetWindowTheme(g_hChkNintendoMode, L"DarkMode_Explorer", NULL);

            g_hChkHairTrigger = CreateWindowW(L"BUTTON", loc.Get(StringId::HairTrigger).c_str(),
                WS_TABSTOP | WS_CHILD | BS_AUTOCHECKBOX,
                S(44), S(256), S(650), S(22), hwnd, (HMENU)(INT_PTR)IDC_CHK_HAIR_TRIGGER, GetModuleHandleW(NULL), NULL);
            SendMessageW(g_hChkHairTrigger, WM_SETFONT, (WPARAM)g_hFontBody, TRUE);
            SendMessageW(g_hChkHairTrigger, BM_SETCHECK, g_hairTrigger ? BST_CHECKED : BST_UNCHECKED, 0);
            SetWindowTheme(g_hChkHairTrigger, L"DarkMode_Explorer", NULL);

            g_hChkDisconnect = CreateWindowW(L"BUTTON", loc.Get(StringId::DisconnectOnExit).c_str(),
                WS_TABSTOP | WS_CHILD | BS_AUTOCHECKBOX,
                S(44), S(286), S(650), S(22), hwnd, (HMENU)(INT_PTR)IDC_CHK_DISCONNECT, GetModuleHandleW(NULL), NULL);
            SendMessageW(g_hChkDisconnect, WM_SETFONT, (WPARAM)g_hFontBody, TRUE);
            SendMessageW(g_hChkDisconnect, BM_SETCHECK, g_disconnectOnExit ? BST_CHECKED : BST_UNCHECKED, 0);
            SetWindowTheme(g_hChkDisconnect, L"DarkMode_Explorer", NULL);

            // Row 1 Buttons: Deadzone & Polling Rate
            g_hBtnDeadzone = CreateWindowW(L"BUTTON", L"",
                WS_TABSTOP | WS_CHILD | BS_OWNERDRAW,
                S(44), S(326), S(340), S(36), hwnd, (HMENU)(INT_PTR)IDC_BTN_DEADZONE, GetModuleHandleW(NULL), NULL);

            g_hBtnPollingRate = CreateWindowW(L"BUTTON", L"",
                WS_TABSTOP | WS_CHILD | BS_OWNERDRAW,
                S(400), S(326), S(340), S(36), hwnd, (HMENU)(INT_PTR)IDC_BTN_POLLING_RATE, GetModuleHandleW(NULL), NULL);

            // Row 2 Buttons: Stick Curve & Back
            g_hBtnCurve = CreateWindowW(L"BUTTON", L"",
                WS_TABSTOP | WS_CHILD | BS_OWNERDRAW,
                S(44), S(374), S(340), S(36), hwnd, (HMENU)(INT_PTR)IDC_BTN_CURVE, GetModuleHandleW(NULL), NULL);

            g_hBtnSettingsBack = CreateWindowW(L"BUTTON", loc.Get(StringId::SettingsBack).c_str(),
                WS_TABSTOP | WS_CHILD | BS_OWNERDRAW,
                S(400), S(374), S(150), S(36), hwnd, (HMENU)(INT_PTR)IDC_BTN_SETTINGS_BACK, GetModuleHandleW(NULL), NULL);

            UpdateDeadzoneButtonText();
            UpdatePollingRateButtonText();
            UpdateCurveButtonText();
            SetupTray(hwnd);
            SetTimer(hwnd, 1, 500, nullptr);   // lets the rate readout fall back to "Idle" when no input arrives

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
            AppendLogId(StringId::LogAppReady);
            // The real controller stays hidden from other programs for as long as this window is open, whether
            // or not the service is running (see ControllerHiding).
            g_hiding = std::make_unique<ControllerHiding>(
                [](const std::wstring& msg, LogLevel level) {
                    auto* pMsg = new std::wstring(msg);
                    PostMessageW(g_hWnd, WM_UPDATE_LOG, (WPARAM)pMsg, (LPARAM)level);
                },
                [](StringId notice) {
                    PostMessageW(g_hWnd, WM_HIDING_NOTICE, (WPARAM)notice, 0);
                });
            g_hiding->Start();
            PostMessageW(hwnd, WM_HIDING_STARTUP_CHECK, 0, 0);   // handled once the window is shown or hidden
            if (isFirstRun && g_driverInstalled) {
                AppendLogId(StringId::DriverReadyFirstRun);
            } else if (!g_driverInstalled) {
                AppendLogId(StringId::DriverMissing);
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
                    SaveUserSettings();
                    break;
                case IDC_CHK_AUTO_START:
                    g_autoStartService = (SendMessageW(g_hChkAutoStart, BM_GETCHECK, 0, 0) == BST_CHECKED);
                    SaveUserSettings();
                    break;
                case IDC_CHK_LOW_BATTERY:
                    g_lowBatteryAlert = (SendMessageW(g_hChkLowBattery, BM_GETCHECK, 0, 0) == BST_CHECKED);
                    SaveUserSettings();
                    break;
                case IDC_CHK_NINTENDO_MODE:
                    g_nintendoMode = (SendMessageW(g_hChkNintendoMode, BM_GETCHECK, 0, 0) == BST_CHECKED);
                    if (g_remapper) g_remapper->SetNintendoMode(g_nintendoMode);
                    SaveUserSettings();
                    InvalidateRect(hwnd, NULL, TRUE);
                    break;
                case IDC_CHK_HAIR_TRIGGER:
                    g_hairTrigger = (SendMessageW(g_hChkHairTrigger, BM_GETCHECK, 0, 0) == BST_CHECKED);
                    if (g_remapper) g_remapper->SetHairTrigger(g_hairTrigger);
                    SaveUserSettings();
                    break;
                case IDC_CHK_DISCONNECT:
                    g_disconnectOnExit = (SendMessageW(g_hChkDisconnect, BM_GETCHECK, 0, 0) == BST_CHECKED);
                    SaveUserSettings();
                    break;
                case IDC_BTN_DEADZONE:
                    g_deadzoneLevel = (g_deadzoneLevel + 1) % 4;
                    UpdateDeadzoneButtonText();
                    if (g_remapper) g_remapper->SetDeadzone(kDeadzoneValues[g_deadzoneLevel]);
                    SaveUserSettings();
                    break;
                case IDC_BTN_POLLING_RATE:
                    g_pollingRateIndex = (g_pollingRateIndex + 1) % 4;
                    UpdatePollingRateButtonText();
                    if (g_remapper) g_remapper->SetPollingRate(kPollingRates[g_pollingRateIndex]);
                    SaveUserSettings();
                    break;
                case IDC_BTN_CURVE:
                    g_responseCurve = (g_responseCurve + 1) % 3;
                    UpdateCurveButtonText();
                    if (g_remapper) g_remapper->SetResponseCurve(g_responseCurve);
                    SaveUserSettings();
                    break;
                case IDC_BTN_START:
                    if (g_driverInstalled) OfferHidingDriverInstall(false);
                    StartServices();
                    break;
                case IDC_BTN_STOP:  StopServices(); break;
                case IDC_BTN_CLEAR_LOGS:
                    g_logLines.clear();
                    SetWindowTextW(g_hEditLogs, L"");
                    break;
                case IDC_BTN_LANG:
                    Localization::Instance().NextLanguage();
                    UpdateUIStrings();
                    SaveUserSettings();
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

        case WM_HIDING_NOTICE:
            ShowTrayBalloon(Localization::Instance().Get(static_cast<StringId>(wParam)));
            return 0;

        case WM_HIDING_STARTUP_CHECK:
            // Hiding is not optional, so a missing HidHide driver must not go unnoticed (also on an automatic start).
            if (!DeviceHider::IsAvailable()) {
                if (IsWindowVisible(hwnd)) {
                    OfferHidingDriverInstall(false);
                } else {
                    g_pendingHidHideOffer = true;
                    ShowTrayBalloon(Localization::Instance().Get(StringId::HideInstallBalloon));
                }
            }
            return 0;

        case WM_TIMER: {
            if (wParam == 1 && !g_showSettings && g_currentStatus == RemapperStatus::Connected &&
                IsWindowVisible(hwnd) && !IsIconic(hwnd)) {
                RECT rcRate = { S(284), S(82), S(470), S(98) };
                InvalidateRect(hwnd, &rcRate, FALSE);
            }
            return 0;
        }

        case WM_UPDATE_INPUT: {
            if (!g_showSettings) {
                RECT rcTelemetry = { S(278), S(56), S(558), S(152) };
                InvalidateRect(hwnd, &rcTelemetry, FALSE);
            }
            return 0;
        }

        case WM_UPDATE_LOG: {
            auto* pMsg = reinterpret_cast<std::wstring*>(wParam);
            if (pMsg) {
                AppendLogMessage(*pMsg, static_cast<LogLevel>(lParam));
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
                XUSB_REPORT zeroInput = {};
                SetLiveInput(zeroInput);
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
            swprintf_s(buf, loc.Get(StringId::DriverDownloadingShort).c_str(), pct);
            SetWindowTextW(g_hBtnStart, buf);
            return 0;
        }

        case WM_DRIVER_INSTALL_DONE: {
            bool success = (wParam == 1);
            g_driverInstalling = false;
            g_driverInstalled = success;
            EnableWindow(g_hBtnStart, TRUE);
            UpdateUIStrings();
            if (success) {
                AppendLogId(StringId::DriverSuccess);
                if (g_autoStartService) {
                    StartServices();
                }
            } else {
                AppendLogId(wParam == 2 ? StringId::LogHideInstallUnverified : StringId::DriverFailed);
            }
            return 0;
        }

        case WM_HIDING_INSTALL_DONE: {
            g_hidingInstalling = false;
            switch (static_cast<HidHideInstallResult>(wParam)) {
                case HidHideInstallResult::Installed:
                    AppendLogId(StringId::LogHideInstalled);
                    break;   // the hiding starts by itself within half a second
                case HidHideInstallResult::NeedsRestart:
                    AppendLogId(StringId::LogHideInstallRestart);
                    break;
                case HidHideInstallResult::DownloadFailed:
                    AppendLogId(StringId::LogHideInstallFailed);
                    break;
                case HidHideInstallResult::NotVerified:
                    AppendLogId(StringId::LogHideInstallUnverified);
                    break;
                case HidHideInstallResult::Cancelled:
                    AppendLogId(StringId::LogHideInstallCancelled);
                    break;
            }
            return 0;
        }

        case WM_CTLCOLORSTATIC:
        case WM_CTLCOLOREDIT: {
            HDC hdcCtrl = (HDC)wParam;
            HWND hwndCtrl = (HWND)lParam;
            if (hwndCtrl == g_hChkMinimizeClose || hwndCtrl == g_hChkStartWindows || hwndCtrl == g_hChkAutoStart || hwndCtrl == g_hChkLowBattery || hwndCtrl == g_hChkNintendoMode || hwndCtrl == g_hChkHairTrigger || hwndCtrl == g_hChkDisconnect) {
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

        case WM_QUERYENDSESSION:
            return TRUE;

        case WM_ENDSESSION:
            if (wParam) {
                // Windows is shutting down or the user is logging off and will end this process right after
                // this message: give the real controller back to other programs first.
                g_sessionEnding = true;   // no permission prompt while Windows shuts down
                StopServices();
                g_hiding.reset();
            }
            return 0;

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
            if (event == NIN_BALLOONUSERCLICK && g_pendingHidHideOffer) {
                g_pendingHidHideOffer = false;
                OfferHidingDriverInstall(true);
                return 0;
            }
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
            KillTimer(hwnd, 1);
            StopServices();
            {
                const DWORD controllerVid = g_hiding ? g_hiding->Vid() : 0;
                const DWORD controllerPid = g_hiding ? g_hiding->Pid() : 0;
                g_hiding.reset();   // gives the real controller back
                // Optional: drop the Bluetooth connection so every program that was using the controller sees it
                // disappear and arrive again. Needs permission (UAC), so only when the user closes the application.
                if (g_disconnectOnExit && !g_sessionEnding) {
                    DisconnectController(controllerVid, controllerPid);
                }
            }
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
            DeleteObject(g_hBrGreen);
            DeleteObject(g_hBrAmber);
            DeleteObject(g_hBrRed);
            DeleteObject(g_hBrButtonNormal);
            DeleteObject(g_hBrStickBg);
            DeleteObject(g_hBrBadgeBg);
            DeleteObject(g_hBrCardBorder);

            DeleteObject(g_hPenCardBorder);
            DeleteObject(g_hPenGreen);
            DeleteObject(g_hPenCross);
            DeleteObject(g_hPenNull);
            PostQuitMessage(0);
            return 0;
        }
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

} // namespace

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow) {
    InitDpiAwareness();
    LoadLibraryW(L"Msftedit.dll");   // rich edit control of the console
    LoadUserSettings();

    g_startWithWindows = CheckStartWithWindows();

    INITCOMMONCONTROLSEX icex = {};
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_STANDARD_CLASSES | ICC_PROGRESS_CLASS;
    InitCommonControlsEx(&icex);

    const wchar_t CLASS_NAME[] = L"Ultimate2CFixer_Class";

    // Only one copy may run: a second one would undo the first one's controller hiding when it starts.
    g_uShowWindowMsg = RegisterWindowMessageW(L"Ultimate2CFixer_ShowWindow");
    HANDLE hSingleInstance = CreateMutexW(nullptr, FALSE, L"Local\\Ultimate2CFixer_SingleInstance");
    if (hSingleInstance && GetLastError() == ERROR_ALREADY_EXISTS) {
        HWND existing = FindWindowW(CLASS_NAME, nullptr);
        if (existing) PostMessageW(existing, g_uShowWindowMsg, 0, 0);
        CloseHandle(hSingleInstance);
        return 0;
    }

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
    int winWidth = MulDiv(840, dpi, 96);
    int winHeight = MulDiv(580, dpi, 96);

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
