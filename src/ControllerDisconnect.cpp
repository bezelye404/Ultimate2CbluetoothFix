#include "ControllerDisconnect.h"
#include "DeviceHider.h"
#include <setupapi.h>
#include <shellapi.h>
#include <algorithm>
#include <vector>

#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "shell32.lib")

namespace Ultimate2CFixer {

namespace {

std::wstring ToUpper(std::wstring s) {
    std::transform(s.begin(), s.end(), s.begin(), [](wchar_t c) { return (wchar_t)towupper(c); });
    return s;
}

bool IsHex(wchar_t c) {
    return (c >= L'0' && c <= L'9') || (c >= L'A' && c <= L'F');
}

// A Bluetooth HID instance id looks like
//   HID\{00001812-0000-1000-8000-00805F9B34FB}_DEV_VID&022DC8_PID&301B_REV&0001_E417D837ECFB\A&18334078&0&0000
// where the last token of the first part is the 12 digit Bluetooth address.
std::wstring BluetoothAddressOf(const std::wstring& hidInstanceId) {
    std::wstring id = ToUpper(hidInstanceId);
    size_t slash = id.find(L'\\');
    if (slash == std::wstring::npos) return L"";
    size_t end = id.find(L'\\', slash + 1);
    if (end == std::wstring::npos) return L"";
    std::wstring first = id.substr(slash + 1, end - slash - 1);
    size_t us = first.rfind(L'_');
    if (us == std::wstring::npos || first.size() - us - 1 != 12) return L"";
    std::wstring addr = first.substr(us + 1);
    return std::all_of(addr.begin(), addr.end(), IsHex) ? addr : L"";
}

} // namespace

std::wstring FindConnectedBluetoothNode(DWORD vid, DWORD pid) {
    // Only the controller's HID entries that are connected right now count.
    std::vector<std::wstring> hidIds = DeviceHider::PresentInstanceIds(vid, pid);
    std::vector<std::wstring> addresses;
    for (const auto& id : hidIds) {
        std::wstring addr = BluetoothAddressOf(id);
        if (!addr.empty() && std::find(addresses.begin(), addresses.end(), addr) == addresses.end()) addresses.push_back(addr);
    }
    if (addresses.empty()) return L"";

    HDEVINFO set = SetupDiGetClassDevsW(nullptr, L"BTHLE", nullptr, DIGCF_ALLCLASSES | DIGCF_PRESENT);
    if (set == INVALID_HANDLE_VALUE) return L"";

    std::wstring found;
    SP_DEVINFO_DATA info = {};
    info.cbSize = sizeof(info);
    for (DWORD i = 0; found.empty() && SetupDiEnumDeviceInfo(set, i, &info); ++i) {
        wchar_t id[512];
        if (!SetupDiGetDeviceInstanceIdW(set, &info, id, 512, nullptr)) continue;
        std::wstring upper = ToUpper(id);
        for (const auto& addr : addresses) {
            if (upper.rfind(L"BTHLE\\DEV_" + addr + L"\\", 0) == 0) {
                found = id;
                break;
            }
        }
    }
    SetupDiDestroyDeviceInfoList(set);
    return found;
}

std::wstring BuildReconnectCommand(const std::wstring& nodeId) {
    // The id comes from Windows, but it ends up on a command line that runs elevated: accept only what a
    // Bluetooth LE device node can look like.
    std::wstring upper = ToUpper(nodeId);
    if (upper.rfind(L"BTHLE\\DEV_", 0) != 0 || nodeId.size() > 200) return L"";
    for (wchar_t c : upper) {
        if (!(IsHex(c) || (c >= L'G' && c <= L'Z') || c == L'\\' || c == L'_' || c == L'&')) return L"";
    }

    wchar_t sys[MAX_PATH];
    UINT n = GetSystemDirectoryW(sys, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) return L"";
    const std::wstring pnputil = std::wstring(L"\"") + sys + L"\\pnputil.exe\"";
    const std::wstring ping = std::wstring(L"\"") + sys + L"\\ping.exe\"";
    const std::wstring id = L"\"" + nodeId + L"\"";

    // /s keeps the quotes of the command line intact; ping is only a short pause between the two steps.
    return L"/s /c \"" + pnputil + L" /disable-device " + id + L" & " + ping + L" -n 4 127.0.0.1 >nul & " +
           pnputil + L" /enable-device " + id + L"\"";
}

bool DisconnectController(DWORD vid, DWORD pid) {
    if (vid == 0) return false;
    std::wstring node = FindConnectedBluetoothNode(vid, pid);
    if (node.empty()) return false;
    std::wstring args = BuildReconnectCommand(node);
    if (args.empty()) return false;

    wchar_t sys[MAX_PATH];
    UINT n = GetSystemDirectoryW(sys, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) return false;
    const std::wstring cmd = std::wstring(sys) + L"\\cmd.exe";

    SHELLEXECUTEINFOW sei = {};
    sei.cbSize = sizeof(sei);
    sei.lpVerb = L"runas";          // permission prompt
    sei.lpFile = cmd.c_str();
    sei.lpParameters = args.c_str();
    sei.nShow = SW_HIDE;
    return ShellExecuteExW(&sei) != FALSE;
}

} // namespace Ultimate2CFixer
