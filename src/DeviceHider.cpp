#include "DeviceHider.h"
#include <setupapi.h>
#include <winioctl.h>
#include <string>
#include <vector>
#include <algorithm>

#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "advapi32.lib")

namespace Ultimate2CFixer {

namespace {

// HidHide control interface (MIT licensed, see THIRD_PARTY_NOTICES.md).
constexpr DWORD kHidHideDeviceType = 32769;
constexpr DWORD kIoctlGetWhitelist = CTL_CODE(kHidHideDeviceType, 2048, METHOD_BUFFERED, FILE_READ_DATA);
constexpr DWORD kIoctlSetWhitelist = CTL_CODE(kHidHideDeviceType, 2049, METHOD_BUFFERED, FILE_READ_DATA);
constexpr DWORD kIoctlGetBlacklist = CTL_CODE(kHidHideDeviceType, 2050, METHOD_BUFFERED, FILE_READ_DATA);
constexpr DWORD kIoctlSetBlacklist = CTL_CODE(kHidHideDeviceType, 2051, METHOD_BUFFERED, FILE_READ_DATA);
constexpr DWORD kIoctlGetActive    = CTL_CODE(kHidHideDeviceType, 2052, METHOD_BUFFERED, FILE_READ_DATA);
constexpr DWORD kIoctlSetActive    = CTL_CODE(kHidHideDeviceType, 2053, METHOD_BUFFERED, FILE_READ_DATA);
constexpr DWORD kIoctlGetWlInverse = CTL_CODE(kHidHideDeviceType, 2054, METHOD_BUFFERED, FILE_READ_DATA);

constexpr wchar_t kStateKey[] = L"Software\\Ultimate2CFixer\\HidHideState";

using StrList = std::vector<std::wstring>;

struct HandleGuard {
    HANDLE h{INVALID_HANDLE_VALUE};
    explicit HandleGuard(HANDLE handle) : h(handle) {}
    ~HandleGuard() { if (h != INVALID_HANDLE_VALUE) CloseHandle(h); }
    HandleGuard(const HandleGuard&) = delete;
    HandleGuard& operator=(const HandleGuard&) = delete;
    bool Valid() const { return h != INVALID_HANDLE_VALUE; }
};

HANDLE OpenHidHide() {
    return CreateFileW(L"\\\\.\\HidHide", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                       nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
}

bool Contains(const StrList& list, const std::wstring& value) {
    return std::any_of(list.begin(), list.end(), [&](const std::wstring& s) {
        return _wcsicmp(s.c_str(), value.c_str()) == 0;
    });
}

// Removes every entry of `values` from `list`. Returns true when the list changed.
bool RemoveAll(StrList& list, const StrList& values) {
    size_t before = list.size();
    list.erase(std::remove_if(list.begin(), list.end(), [&](const std::wstring& s) {
        return Contains(values, s);
    }), list.end());
    return list.size() != before;
}

bool IoGetList(HANDLE h, DWORD code, StrList& out) {
    std::vector<wchar_t> buf(4096);
    for (int attempt = 0; attempt < 6; ++attempt) {
        DWORD bytes = 0;
        BOOL ok = DeviceIoControl(h, code, nullptr, 0, buf.data(),
                                  static_cast<DWORD>(buf.size() * sizeof(wchar_t)), &bytes, nullptr);
        if (ok) {
            size_t n = (std::min)(static_cast<size_t>(bytes / sizeof(wchar_t)), buf.size());
            out.clear();
            size_t i = 0;
            while (i < n && buf[i] != L'\0') {
                size_t start = i;
                while (i < n && buf[i] != L'\0') ++i;
                out.emplace_back(&buf[start], i - start);
                ++i;
            }
            return true;
        }
        DWORD err = GetLastError();
        if (err != ERROR_INSUFFICIENT_BUFFER && err != ERROR_MORE_DATA) return false;
        size_t need = static_cast<size_t>(bytes / sizeof(wchar_t)) + 2;
        buf.resize((std::max)(buf.size() * 4, need));
    }
    return false;
}

bool IoSetList(HANDLE h, DWORD code, const StrList& list) {
    std::vector<wchar_t> buf;
    for (const auto& s : list) {
        buf.insert(buf.end(), s.begin(), s.end());
        buf.push_back(L'\0');
    }
    buf.push_back(L'\0');
    if (list.empty()) buf.push_back(L'\0');
    DWORD bytes = 0;
    return DeviceIoControl(h, code, buf.data(), static_cast<DWORD>(buf.size() * sizeof(wchar_t)),
                           nullptr, 0, &bytes, nullptr) != FALSE;
}

bool IoGetBool(HANDLE h, DWORD code, BOOLEAN& value) {
    DWORD bytes = 0;
    value = FALSE;
    return DeviceIoControl(h, code, nullptr, 0, &value, sizeof(value), &bytes, nullptr) != FALSE;
}

bool IoSetBool(HANDLE h, DWORD code, BOOLEAN value) {
    DWORD bytes = 0;
    return DeviceIoControl(h, code, &value, sizeof(value), nullptr, 0, &bytes, nullptr) != FALSE;
}

// HidHide identifies applications by their NT path (\Device\HarddiskVolumeN\...).
bool GetOwnNtPath(std::wstring& out) {
    wchar_t exe[MAX_PATH];
    DWORD len = GetModuleFileNameW(nullptr, exe, MAX_PATH);
    if (len == 0 || len >= MAX_PATH || exe[1] != L':') return false;

    wchar_t drive[3] = { exe[0], L':', L'\0' };
    wchar_t device[MAX_PATH];
    if (QueryDosDeviceW(drive, device, MAX_PATH) == 0) return false;

    out = std::wstring(device) + (exe + 2);
    return true;
}

// Locates the HID device instances of the physical controller.
StrList FindHidInstanceIds(DWORD vid, DWORD pid) {
    StrList result;
    // GUID_DEVINTERFACE_HID
    static const GUID kHidInterface = { 0x4D1E55B2, 0xF16F, 0x11CF, { 0x88, 0xCB, 0x00, 0x11, 0x11, 0x00, 0x00, 0x30 } };

    HDEVINFO set = SetupDiGetClassDevsW(&kHidInterface, nullptr, nullptr, DIGCF_DEVICEINTERFACE | DIGCF_PRESENT);
    if (set == INVALID_HANDLE_VALUE) return result;

    wchar_t vidStr[8], pidStr[8];
    swprintf_s(vidStr, L"%04X", vid & 0xFFFF);
    swprintf_s(pidStr, L"%04X", pid & 0xFFFF);

    SP_DEVINFO_DATA info = {};
    info.cbSize = sizeof(info);
    for (DWORD i = 0; SetupDiEnumDeviceInfo(set, i, &info); ++i) {
        wchar_t id[512];
        if (!SetupDiGetDeviceInstanceIdW(set, &info, id, 512, nullptr)) continue;

        std::wstring upper(id);
        std::transform(upper.begin(), upper.end(), upper.begin(), [](wchar_t c) { return (wchar_t)towupper(c); });
        if (upper.rfind(L"HID\\", 0) != 0) continue;
        if (upper.find(vidStr) == std::wstring::npos || upper.find(pidStr) == std::wstring::npos) continue;

        if (!Contains(result, id)) result.emplace_back(id);
        if (result.size() >= 8) break;
    }
    SetupDiDestroyDeviceInfoList(set);
    return result;
}

// MARK: - Ownership record
// What this application added is persisted so a crash can be cleaned up on the next launch.

struct OwnedEntries {
    StrList blacklist;          // entries this app added to the blacklist
    std::wstring whitelist;     // this app's own path, when this app added it
    bool activeChanged{false};  // this app switched HidHide on
    StrList baseline;           // blacklist entries that existed before this app added anything
    bool hasBaseline{false};    // false for records written by older versions
};

std::vector<wchar_t> ToMultiSz(const StrList& list) {
    std::vector<wchar_t> multi;
    for (const auto& s : list) {
        multi.insert(multi.end(), s.begin(), s.end());
        multi.push_back(L'\0');
    }
    multi.push_back(L'\0');
    if (list.empty()) multi.push_back(L'\0');
    return multi;
}

bool WriteMultiSz(HKEY key, const wchar_t* name, const StrList& list) {
    std::vector<wchar_t> multi = ToMultiSz(list);
    return RegSetValueExW(key, name, 0, REG_MULTI_SZ, reinterpret_cast<const BYTE*>(multi.data()),
                          static_cast<DWORD>(multi.size() * sizeof(wchar_t))) == ERROR_SUCCESS;
}

// Returns false when the value does not exist.
bool ReadMultiSz(HKEY key, const wchar_t* name, StrList& out) {
    out.clear();
    DWORD size = 0;
    if (RegQueryValueExW(key, name, nullptr, nullptr, nullptr, &size) != ERROR_SUCCESS) return false;
    if (size < sizeof(wchar_t)) return true;
    std::vector<wchar_t> buf(size / sizeof(wchar_t) + 2, L'\0');
    DWORD got = size;
    if (RegQueryValueExW(key, name, nullptr, nullptr, reinterpret_cast<LPBYTE>(buf.data()), &got) != ERROR_SUCCESS) return false;
    size_t n = got / sizeof(wchar_t);
    size_t i = 0;
    while (i < n && buf[i] != L'\0') {
        size_t start = i;
        while (i < n && buf[i] != L'\0') ++i;
        out.emplace_back(&buf[start], i - start);
        ++i;
    }
    return true;
}

bool SaveOwned(const OwnedEntries& owned) {
    HKEY key;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, kStateKey, 0, nullptr, 0, KEY_WRITE, nullptr, &key, nullptr) != ERROR_SUCCESS) {
        return false;
    }
    bool ok = true;
    ok &= WriteMultiSz(key, L"Blacklist", owned.blacklist);
    ok &= WriteMultiSz(key, L"Baseline", owned.baseline);
    ok &= RegSetValueExW(key, L"Whitelist", 0, REG_SZ, reinterpret_cast<const BYTE*>(owned.whitelist.c_str()),
                         static_cast<DWORD>((owned.whitelist.size() + 1) * sizeof(wchar_t))) == ERROR_SUCCESS;
    DWORD flag = owned.activeChanged ? 1 : 0;
    ok &= RegSetValueExW(key, L"ActiveChanged", 0, REG_DWORD, reinterpret_cast<const BYTE*>(&flag), sizeof(flag)) == ERROR_SUCCESS;
    RegCloseKey(key);
    return ok;
}

bool LoadOwned(OwnedEntries& owned) {
    HKEY key;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kStateKey, 0, KEY_READ, &key) != ERROR_SUCCESS) return false;

    owned = OwnedEntries{};
    ReadMultiSz(key, L"Blacklist", owned.blacklist);
    owned.hasBaseline = ReadMultiSz(key, L"Baseline", owned.baseline);

    DWORD size = 0;
    if (RegQueryValueExW(key, L"Whitelist", nullptr, nullptr, nullptr, &size) == ERROR_SUCCESS && size >= sizeof(wchar_t)) {
        std::vector<wchar_t> buf(size / sizeof(wchar_t) + 1, L'\0');
        DWORD got = size;
        if (RegQueryValueExW(key, L"Whitelist", nullptr, nullptr, reinterpret_cast<LPBYTE>(buf.data()), &got) == ERROR_SUCCESS) {
            owned.whitelist = buf.data();
        }
    }

    DWORD flag = 0;
    size = sizeof(flag);
    if (RegQueryValueExW(key, L"ActiveChanged", nullptr, nullptr, reinterpret_cast<LPBYTE>(&flag), &size) == ERROR_SUCCESS) {
        owned.activeChanged = (flag != 0);
    }
    RegCloseKey(key);
    return true;
}

void ClearOwned() {
    RegDeleteKeyW(HKEY_CURRENT_USER, kStateKey);
}

// Undoes exactly what the persisted record says this application added.
bool RestoreOwned() {
    OwnedEntries owned;
    if (!LoadOwned(owned)) return false;

    HandleGuard dev(OpenHidHide());
    if (!dev.Valid()) return false; // Keep the record; retry on the next launch.

    bool ok = true;

    StrList black;
    bool haveBlack = false;
    if (!owned.blacklist.empty() || owned.activeChanged) {
        haveBlack = IoGetList(dev.h, kIoctlGetBlacklist, black);
        if (!haveBlack) ok = false;
    }
    if (haveBlack && !owned.blacklist.empty()) {
        if (RemoveAll(black, owned.blacklist)) ok &= IoSetList(dev.h, kIoctlSetBlacklist, black);
    }

    if (!owned.whitelist.empty()) {
        StrList white;
        if (IoGetList(dev.h, kIoctlGetWhitelist, white)) {
            if (RemoveAll(white, StrList{ owned.whitelist })) ok &= IoSetList(dev.h, kIoctlSetWhitelist, white);
        } else {
            ok = false;
        }
    }

    if (owned.activeChanged && haveBlack) {
        // Switch HidHide off again only when nothing else started relying on it meanwhile: every entry that is
        // still in the blacklist must have been there before this app added anything (another tool such as
        // DS4Windows may have added its own entries and needs HidHide to stay on).
        bool othersAdded;
        if (owned.hasBaseline) {
            othersAdded = std::any_of(black.begin(), black.end(), [&](const std::wstring& s) { return !Contains(owned.baseline, s); });
        } else {
            othersAdded = !black.empty(); // record from an older version: be conservative
        }
        if (!othersAdded) ok &= IoSetBool(dev.h, kIoctlSetActive, FALSE);
    }

    if (ok) ClearOwned();
    return ok;
}

} // namespace

bool DeviceHider::IsAvailable() {
    HandleGuard dev(OpenHidHide());
    return dev.Valid();
}

bool DeviceHider::RecoverStale() {
    return RestoreOwned();
}

HideResult DeviceHider::Hide(DWORD vid, DWORD pid) {
    if (m_hidden) return HideResult::Hidden;

    HandleGuard dev(OpenHidHide());
    if (!dev.Valid()) return HideResult::NotInstalled;

    StrList ids = FindHidInstanceIds(vid, pid);
    if (ids.empty()) return HideResult::DeviceNotFound;

    // An inverted application list changes the meaning of every entry; leave such setups alone.
    BOOLEAN inverse = FALSE;
    if (IoGetBool(dev.h, kIoctlGetWlInverse, inverse) && inverse) return HideResult::UnsupportedSetup;

    StrList white, black;
    BOOLEAN active = FALSE;
    std::wstring ntPath;
    if (!IoGetList(dev.h, kIoctlGetWhitelist, white) ||
        !IoGetList(dev.h, kIoctlGetBlacklist, black) ||
        !IoGetBool(dev.h, kIoctlGetActive, active) ||
        !GetOwnNtPath(ntPath)) {
        return HideResult::Failed;
    }

    OwnedEntries owned;
    for (const auto& id : ids) {
        if (!Contains(black, id)) owned.blacklist.push_back(id);
    }
    if (!Contains(white, ntPath)) owned.whitelist = ntPath;
    owned.activeChanged = !active;
    owned.baseline = black;
    owned.hasBaseline = true;

    if (owned.blacklist.empty() && owned.whitelist.empty() && !owned.activeChanged) {
        m_hidden = true;
        return HideResult::AlreadyHidden;
    }

    // Record first, so a crash between the steps below can still be cleaned up.
    if (!SaveOwned(owned)) return HideResult::Failed;

    bool ok = true;
    if (!owned.whitelist.empty()) {
        white.push_back(owned.whitelist);
        ok &= IoSetList(dev.h, kIoctlSetWhitelist, white);
    }
    if (ok && !owned.blacklist.empty()) {
        black.insert(black.end(), owned.blacklist.begin(), owned.blacklist.end());
        ok &= IoSetList(dev.h, kIoctlSetBlacklist, black);
    }
    if (ok && owned.activeChanged) {
        ok &= IoSetBool(dev.h, kIoctlSetActive, TRUE);
    }

    if (!ok) {
        RestoreOwned();
        return HideResult::Failed;
    }

    m_hidden = true;
    return HideResult::Hidden;
}

bool DeviceHider::Restore() {
    bool restored = RestoreOwned();
    m_hidden = false;
    return restored;
}

} // namespace Ultimate2CFixer
