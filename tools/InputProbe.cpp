// Developer diagnostic. Read-only: it never changes any setting or device state.
//   InputProbe hidhide        show the current HidHide configuration and the state recorded by this app
//   InputProbe hid            list the HID inputs (usage, range) the 8BitDo exposes to Windows
//   InputProbe vigem          plug a virtual pad, feed trigger values 0..255 and read them back through XInput
//   InputProbe dinput [secs]  show the DirectInput objects of the 8BitDo and, when secs > 0, sample it
//                             for that long (pull both triggers slowly, press every button once)
// Build: cmake --build build --config Release --target InputProbe
#define DIRECTINPUT_VERSION 0x0800
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winioctl.h>
#include <dinput.h>
#include <setupapi.h>
extern "C" {
#include <hidsdi.h>
}
#include <hidpi.h>
#include <xinput.h>
#include "ViGEm/Client.h"
#include <cstdio>
#include <cstdarg>
#include <string>
#include <vector>
#include <set>
#include <algorithm>

#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "hid.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "xinput.lib")

namespace {

void Out(const wchar_t* fmt, ...) {
    wchar_t buf[2048];
    va_list ap;
    va_start(ap, fmt);
    vswprintf_s(buf, fmt, ap);
    va_end(ap);
    char utf8[8192];
    int n = WideCharToMultiByte(CP_UTF8, 0, buf, -1, utf8, sizeof(utf8), nullptr, nullptr);
    if (n > 1) fwrite(utf8, 1, n - 1, stdout);
    fflush(stdout);
}

constexpr DWORD kHidHideType = 32769;
constexpr DWORD kGetWhitelist = CTL_CODE(kHidHideType, 2048, METHOD_BUFFERED, FILE_READ_DATA);
constexpr DWORD kGetBlacklist = CTL_CODE(kHidHideType, 2050, METHOD_BUFFERED, FILE_READ_DATA);
constexpr DWORD kGetActive    = CTL_CODE(kHidHideType, 2052, METHOD_BUFFERED, FILE_READ_DATA);
constexpr DWORD kGetInverse   = CTL_CODE(kHidHideType, 2054, METHOD_BUFFERED, FILE_READ_DATA);

std::vector<std::wstring> GetList(HANDLE h, DWORD code, DWORD* err) {
    std::vector<std::wstring> out;
    std::vector<wchar_t> buf(8192);
    DWORD bytes = 0;
    if (!DeviceIoControl(h, code, nullptr, 0, buf.data(), (DWORD)(buf.size() * 2), &bytes, nullptr)) {
        *err = GetLastError();
        return out;
    }
    *err = 0;
    size_t n = bytes / 2, i = 0;
    while (i < n && buf[i]) {
        size_t s = i;
        while (i < n && buf[i]) ++i;
        out.emplace_back(&buf[s], i - s);
        ++i;
    }
    return out;
}

void ShowHidHide() {
    HANDLE h = CreateFileW(L"\\\\.\\HidHide", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
    if (h == INVALID_HANDLE_VALUE) {
        Out(L"HidHide control device could not be opened (error %lu).\n", GetLastError());
    } else {
        BOOLEAN b = 0;
        DWORD bytes = 0;
        BOOL ok = DeviceIoControl(h, kGetActive, nullptr, 0, &b, sizeof(b), &bytes, nullptr);
        Out(L"Active   : %s\n", ok ? (b ? L"yes" : L"no") : L"(read failed)");
        ok = DeviceIoControl(h, kGetInverse, nullptr, 0, &b, sizeof(b), &bytes, nullptr);
        Out(L"Inverse  : %s\n", ok ? (b ? L"yes" : L"no") : L"(read failed)");
        DWORD err = 0;
        auto white = GetList(h, kGetWhitelist, &err);
        Out(L"Whitelist: %zu entries%s\n", white.size(), err ? L" (READ FAILED)" : L"");
        for (auto& s : white) Out(L"   %s\n", s.c_str());
        auto black = GetList(h, kGetBlacklist, &err);
        Out(L"Blacklist: %zu entries%s\n", black.size(), err ? L" (READ FAILED)" : L"");
        for (auto& s : black) Out(L"   %s\n", s.c_str());
        CloseHandle(h);
    }

    HKEY key;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Ultimate2CFixer\\HidHideState", 0, KEY_READ, &key) == ERROR_SUCCESS) {
        Out(L"\nThis app has an unrestored hiding record (it is cleaned up on the next app start):\n");
        wchar_t buf[1024] = {};
        DWORD size = sizeof(buf) - 2;
        if (RegQueryValueExW(key, L"Blacklist", nullptr, nullptr, (LPBYTE)buf, &size) == ERROR_SUCCESS) Out(L"   recorded blacklist entry: %s\n", buf);
        size = sizeof(buf) - 2;
        wchar_t w[1024] = {};
        if (RegQueryValueExW(key, L"Whitelist", nullptr, nullptr, (LPBYTE)w, &size) == ERROR_SUCCESS) Out(L"   recorded whitelist entry: %s\n", w);
        RegCloseKey(key);
    } else {
        Out(L"\nNo unrestored hiding record from this app.\n");
    }
}

// MARK: - HID
void ShowHid() {
    static const GUID kHid = { 0x4D1E55B2, 0xF16F, 0x11CF, { 0x88, 0xCB, 0x00, 0x11, 0x11, 0x00, 0x00, 0x30 } };
    HDEVINFO set = SetupDiGetClassDevsW(&kHid, nullptr, nullptr, DIGCF_DEVICEINTERFACE | DIGCF_PRESENT);
    if (set == INVALID_HANDLE_VALUE) { Out(L"SetupDi failed\n"); return; }
    int found = 0;
    SP_DEVICE_INTERFACE_DATA ifd = {};
    ifd.cbSize = sizeof(ifd);
    for (DWORD i = 0; SetupDiEnumDeviceInterfaces(set, nullptr, &kHid, i, &ifd); ++i) {
        DWORD need = 0;
        SetupDiGetDeviceInterfaceDetailW(set, &ifd, nullptr, 0, &need, nullptr);
        std::vector<BYTE> raw(need);
        auto* detail = reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W*>(raw.data());
        detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);
        if (!SetupDiGetDeviceInterfaceDetailW(set, &ifd, detail, need, nullptr, nullptr)) continue;

        HANDLE h = CreateFileW(detail->DevicePath, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
        if (h == INVALID_HANDLE_VALUE) continue;
        HIDD_ATTRIBUTES attr = { sizeof(attr) };
        if (!HidD_GetAttributes(h, &attr) || attr.VendorID != 0x2DC8) { CloseHandle(h); continue; }
        ++found;
        Out(L"\nHID device VID %04X PID %04X\n  %s\n", attr.VendorID, attr.ProductID, detail->DevicePath);

        PHIDP_PREPARSED_DATA pp = nullptr;
        if (HidD_GetPreparsedData(h, &pp)) {
            HIDP_CAPS caps = {};
            HidP_GetCaps(pp, &caps);
            Out(L"  top-level usage page 0x%04X usage 0x%04X, input report %u bytes\n", caps.UsagePage, caps.Usage, caps.InputReportByteLength);

            USHORT n = caps.NumberInputValueCaps;
            std::vector<HIDP_VALUE_CAPS> vc(n);
            if (n && HidP_GetValueCaps(HidP_Input, vc.data(), &n, pp) == HIDP_STATUS_SUCCESS) {
                Out(L"  input value fields (%u):\n", n);
                for (auto& c : vc) {
                    if (c.IsRange) {
                        Out(L"    page 0x%04X usages 0x%02X-0x%02X  logical %ld..%ld  bits %u  report %u\n", c.UsagePage,
                            c.Range.UsageMin, c.Range.UsageMax, c.LogicalMin, c.LogicalMax, c.BitSize, c.ReportID);
                    } else {
                        Out(L"    page 0x%04X usage  0x%02X       logical %ld..%ld  bits %u  report %u\n", c.UsagePage,
                            c.NotRange.Usage, c.LogicalMin, c.LogicalMax, c.BitSize, c.ReportID);
                    }
                }
            }
            USHORT nb = caps.NumberInputButtonCaps;
            std::vector<HIDP_BUTTON_CAPS> bc(nb);
            if (nb && HidP_GetButtonCaps(HidP_Input, bc.data(), &nb, pp) == HIDP_STATUS_SUCCESS) {
                for (auto& c : bc) {
                    if (c.IsRange) Out(L"  buttons: page 0x%04X usages %u-%u\n", c.UsagePage, c.Range.UsageMin, c.Range.UsageMax);
                    else Out(L"  button : page 0x%04X usage %u\n", c.UsagePage, c.NotRange.Usage);
                }
            }
            HidD_FreePreparsedData(pp);
        } else {
            Out(L"  (preparsed data not readable: the device may be hidden from this tool)\n");
        }
        CloseHandle(h);
    }
    SetupDiDestroyDeviceInfoList(set);
    if (!found) Out(L"No 8BitDo HID device is visible to this tool (controller off, or hidden by HidHide).\n");
}

// MARK: - DirectInput
const wchar_t* GuidName(const GUID& g) {
    if (IsEqualGUID(g, GUID_XAxis)) return L"X axis";
    if (IsEqualGUID(g, GUID_YAxis)) return L"Y axis";
    if (IsEqualGUID(g, GUID_ZAxis)) return L"Z axis";
    if (IsEqualGUID(g, GUID_RxAxis)) return L"Rx axis";
    if (IsEqualGUID(g, GUID_RyAxis)) return L"Ry axis";
    if (IsEqualGUID(g, GUID_RzAxis)) return L"Rz axis";
    if (IsEqualGUID(g, GUID_Slider)) return L"Slider";
    if (IsEqualGUID(g, GUID_Button)) return L"Button";
    if (IsEqualGUID(g, GUID_POV)) return L"POV";
    return L"other";
}

struct Found { GUID guid; std::wstring name; DWORD vid, pid; };

BOOL CALLBACK EnumDev(LPCDIDEVICEINSTANCEW d, LPVOID ref) {
    auto* v = reinterpret_cast<std::vector<Found>*>(ref);
    v->push_back({ d->guidInstance, d->tszProductName, LOWORD(d->guidProduct.Data1), HIWORD(d->guidProduct.Data1) });
    return DIENUM_CONTINUE;
}

struct ObjCtx { IDirectInputDevice8W* dev; };

BOOL CALLBACK EnumObj(LPCDIDEVICEOBJECTINSTANCEW o, LPVOID ref) {
    auto* ctx = reinterpret_cast<ObjCtx*>(ref);
    wchar_t range[64] = L"";
    if (o->dwType & DIDFT_AXIS) {
        DIPROPRANGE pr = {};
        pr.diph.dwSize = sizeof(pr);
        pr.diph.dwHeaderSize = sizeof(DIPROPHEADER);
        pr.diph.dwObj = o->dwType;
        pr.diph.dwHow = DIPH_BYID;
        if (SUCCEEDED(ctx->dev->GetProperty(DIPROP_RANGE, &pr.diph))) swprintf_s(range, L"range %ld..%ld", pr.lMin, pr.lMax);
    }
    Out(L"  %-10s offset %3lu  type 0x%08lX  %-18s %s\n", GuidName(o->guidType), o->dwOfs, o->dwType, o->tszName, range);
    return DIENUM_CONTINUE;
}

struct Stat {
    LONG first = 0, min = 0, max = 0;
    bool seen = false;
    std::set<LONG> distinct;
    void Add(LONG v) {
        if (!seen) { first = min = max = v; seen = true; }
        min = (std::min)(min, v);
        max = (std::max)(max, v);
        if (distinct.size() < 4096) distinct.insert(v);
    }
};

void ShowDirectInput(int seconds) {
    IDirectInput8W* di = nullptr;
    if (FAILED(DirectInput8Create(GetModuleHandleW(nullptr), DIRECTINPUT_VERSION, IID_IDirectInput8W, (void**)&di, nullptr))) {
        Out(L"DirectInput8Create failed\n");
        return;
    }
    std::vector<Found> devs;
    di->EnumDevices(DI8DEVCLASS_GAMECTRL, EnumDev, &devs, DIEDFL_ATTACHEDONLY);
    Out(L"Attached game controllers: %zu\n", devs.size());
    const Found* target = nullptr;
    for (auto& d : devs) {
        Out(L"  %s  (VID %04lX PID %04lX)\n", d.name.c_str(), d.vid, d.pid);
        if (d.vid == 0x2DC8 && !target) target = &d;
    }
    if (!target) {
        Out(L"\nNo 8BitDo controller is visible to DirectInput (controller off or hidden).\n");
        di->Release();
        return;
    }

    IDirectInputDevice8W* dev = nullptr;
    if (FAILED(di->CreateDevice(target->guid, &dev, nullptr)) || FAILED(dev->SetDataFormat(&c_dfDIJoystick2))) {
        Out(L"Could not open the device\n");
        di->Release();
        return;
    }
    dev->SetCooperativeLevel(GetDesktopWindow(), DISCL_BACKGROUND | DISCL_NONEXCLUSIVE);

    DIDEVCAPS caps = { sizeof(caps) };
    dev->GetCapabilities(&caps);
    Out(L"\nCapabilities: %lu axes, %lu buttons, %lu POV hats\n", caps.dwAxes, caps.dwButtons, caps.dwPOVs);
    Out(L"Objects:\n");
    ObjCtx ctx{ dev };
    dev->EnumObjects(EnumObj, &ctx, DIDFT_ALL);

    if (seconds <= 0) { dev->Release(); di->Release(); return; }

    if (FAILED(dev->Acquire())) { Out(L"Acquire failed\n"); dev->Release(); di->Release(); return; }
    Out(L"\nSampling for %d s: pull LT slowly all the way, release, then RT, then press every button once...\n", seconds);

    const wchar_t* names[] = { L"lX", L"lY", L"lZ", L"lRx", L"lRy", L"lRz", L"slider0", L"slider1" };
    Stat st[8];
    int pressCount[16] = {};
    bool prevBtn[16] = {};
    // Values of the candidate trigger sources at the first frame button 8 / 9 went down.
    bool captured[2] = {};
    LONG atClick[2][8] = {};
    LONG lastVals[8] = {};
    ULONGLONG endTick = GetTickCount64() + (ULONGLONG)seconds * 1000;
    DWORD frames = 0;
    while (GetTickCount64() < endTick) {
        dev->Poll();
        DIJOYSTATE2 s = {};
        if (FAILED(dev->GetDeviceState(sizeof(s), &s))) { dev->Acquire(); Sleep(2); continue; }
        LONG v[8] = { s.lX, s.lY, s.lZ, s.lRx, s.lRy, s.lRz, s.rglSlider[0], s.rglSlider[1] };
        for (int i = 0; i < 8; ++i) { st[i].Add(v[i]); lastVals[i] = v[i]; }
        for (int b = 0; b < 16; ++b) {
            bool down = s.rgbButtons[b] != 0;
            if (down && !prevBtn[b]) {
                ++pressCount[b];
                if (b == 8 || b == 9) {
                    int k = b - 8;
                    if (!captured[k]) { captured[k] = true; for (int i = 0; i < 8; ++i) atClick[k][i] = v[i]; }
                }
            }
            prevBtn[b] = down;
        }
        ++frames;
        Sleep(2);
    }

    Out(L"\n%lu samples. Axis summary (first / min..max / distinct values seen):\n", frames);
    for (int i = 0; i < 8; ++i) {
        Out(L"  %-8s first %6ld   %6ld..%-6ld   %zu distinct\n", names[i], st[i].first, st[i].min, st[i].max, st[i].distinct.size());
    }
    Out(L"\nButton presses seen: ");
    for (int b = 0; b < 16; ++b) if (pressCount[b]) Out(L"[%d]x%d ", b, pressCount[b]);
    Out(L"\n");
    for (int k = 0; k < 2; ++k) {
        if (!captured[k]) { Out(L"Button %d (trigger click) was never pressed.\n", 8 + k); continue; }
        Out(L"At the first frame of button %d going down:", 8 + k);
        for (int i = 0; i < 8; ++i) Out(L"  %s=%ld", names[i], atClick[k][i]);
        Out(L"\n");
    }
    dev->Unacquire();
    dev->Release();
    di->Release();
}

// MARK: - ViGEm pass-through test (needs no controller)
void TestVigem() {
    PVIGEM_CLIENT client = vigem_alloc();
    if (!client || !VIGEM_SUCCESS(vigem_connect(client))) {
        Out(L"Could not connect to ViGEmBus\n");
        return;
    }
    PVIGEM_TARGET pad = vigem_target_x360_alloc();

    if (!VIGEM_SUCCESS(vigem_target_add(client, pad))) {
        Out(L"Could not plug the virtual pad\n");
        vigem_target_free(pad);
        vigem_free(client);
        return;
    }
    Sleep(800);

    ULONG idx = 0;
    if (!VIGEM_SUCCESS(vigem_target_x360_get_user_index(client, pad, &idx))) Out(L"No XInput slot reported\n");
    Out(L"Virtual pad is XInput slot %lu\n", idx);

    const BYTE values[] = { 0, 1, 10, 37, 64, 100, 128, 170, 200, 240, 254, 255 };
    int bad = 0;
    for (BYTE v : values) {
        XUSB_REPORT r = {};
        r.bLeftTrigger = v;
        r.bRightTrigger = (BYTE)(255 - v);
        vigem_target_x360_update(client, pad, r);
        Sleep(40);
        XINPUT_STATE st = {};
        DWORD res = XInputGetState(idx, &st);
        bool ok = res == ERROR_SUCCESS && st.Gamepad.bLeftTrigger == v && st.Gamepad.bRightTrigger == (BYTE)(255 - v);
        if (!ok) ++bad;
        Out(L"  sent LT %3u RT %3u  ->  XInput LT %3u RT %3u  %s\n", v, 255 - v, st.Gamepad.bLeftTrigger, st.Gamepad.bRightTrigger, ok ? L"ok" : L"MISMATCH");
    }
    if (bad) Out(L"RESULT: %d values did not pass through\n", bad);
    else Out(L"RESULT: every value passed through unchanged (ViGEm keeps triggers analog)\n");

    vigem_target_remove(client, pad);
    vigem_target_free(pad);
    vigem_disconnect(client);
    vigem_free(client);
}

} // namespace

int wmain(int argc, wchar_t** argv) {
    SetConsoleOutputCP(CP_UTF8);
    std::wstring mode = argc > 1 ? argv[1] : L"";
    if (mode == L"hidhide") ShowHidHide();
    else if (mode == L"hid") ShowHid();
    else if (mode == L"vigem") TestVigem();
    else if (mode == L"dinput") ShowDirectInput(argc > 2 ? _wtoi(argv[2]) : 0);
    else Out(L"usage: InputProbe hidhide | hid | vigem | dinput [seconds]\n");
    return 0;
}
