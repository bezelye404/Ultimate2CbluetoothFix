#define DIRECTINPUT_VERSION 0x0800
#include "Remapper.h"
#include "Localization.h"
#include <dinput.h>
#include <vector>
#include <algorithm>
#include <cwctype>

#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")

namespace BitDoFixer {

namespace {
    constexpr int Deadzone = 4000;
    constexpr int KeepAliveTicks = 50; // ~250ms

    struct DeviceChoice {
        GUID guid;
        std::wstring name;
        bool is8BitDo;
    };

    std::wstring ToLower(std::wstring str) {
        std::transform(str.begin(), str.end(), str.begin(), [](wchar_t c) { return (wchar_t)std::towlower(c); });
        return str;
    }

    BOOL CALLBACK EnumDevicesCallback(LPCDIDEVICEINSTANCEW lpddi, LPVOID pvRef) {
        auto* list = reinterpret_cast<std::vector<DeviceChoice>*>(pvRef);
        std::wstring name = lpddi->tszInstanceName;
        std::wstring lower = ToLower(name);
        bool is8 = (lower.find(L"8bitdo") != std::wstring::npos);

        list->push_back({ lpddi->guidInstance, name, is8 });
        return DIENUM_CONTINUE;
    }
}

Remapper::Remapper() = default;

Remapper::~Remapper() {
    Stop();
}

bool Remapper::InitViGEm() {
    m_hViGEmDll = LoadLibraryW(L"ViGEmClient.dll");
    if (!m_hViGEmDll) {
        if (m_logCallback) m_logCallback(L"ERROR: ViGEmClient.dll could not be loaded. Please ensure ViGEmBus is installed.");
        return false;
    }

    m_fn_alloc = (pfn_vigem_alloc)GetProcAddress(m_hViGEmDll, "vigem_alloc");
    m_fn_free = (pfn_vigem_free)GetProcAddress(m_hViGEmDll, "vigem_free");
    m_fn_connect = (pfn_vigem_connect)GetProcAddress(m_hViGEmDll, "vigem_connect");
    m_fn_disconnect = (pfn_vigem_disconnect)GetProcAddress(m_hViGEmDll, "vigem_disconnect");
    m_fn_target_alloc = (pfn_vigem_target_x360_alloc)GetProcAddress(m_hViGEmDll, "vigem_target_x360_alloc");
    m_fn_target_free = (pfn_vigem_target_free)GetProcAddress(m_hViGEmDll, "vigem_target_free");
    m_fn_target_add = (pfn_vigem_target_add)GetProcAddress(m_hViGEmDll, "vigem_target_add");
    m_fn_target_remove = (pfn_vigem_target_remove)GetProcAddress(m_hViGEmDll, "vigem_target_remove");
    m_fn_target_update = (pfn_vigem_target_x360_update)GetProcAddress(m_hViGEmDll, "vigem_target_x360_update");

    if (!m_fn_alloc || !m_fn_connect || !m_fn_target_alloc || !m_fn_target_add || !m_fn_target_update) {
        if (m_logCallback) m_logCallback(L"ERROR: Incompatible ViGEmClient.dll exports.");
        FreeLibrary(m_hViGEmDll);
        m_hViGEmDll = nullptr;
        return false;
    }

    m_vigemClient = m_fn_alloc();
    if (!m_vigemClient) return false;

    VIGEM_ERROR err = m_fn_connect(m_vigemClient);
    if (!VIGEM_SUCCESS(err)) {
        if (m_logCallback) m_logCallback(L"ERROR: Could not connect to ViGEmBus driver.");
        m_fn_free(m_vigemClient);
        m_vigemClient = nullptr;
        return false;
    }

    m_vigemTarget = m_fn_target_alloc();
    if (!m_vigemTarget) {
        m_fn_disconnect(m_vigemClient);
        m_fn_free(m_vigemClient);
        m_vigemClient = nullptr;
        return false;
    }

    err = m_fn_target_add(m_vigemClient, m_vigemTarget);
    if (!VIGEM_SUCCESS(err)) {
        m_fn_target_free(m_vigemTarget);
        m_fn_disconnect(m_vigemClient);
        m_fn_free(m_vigemClient);
        m_vigemTarget = nullptr;
        m_vigemClient = nullptr;
        return false;
    }

    return true;
}

void Remapper::UninitViGEm() {
    if (m_vigemClient && m_vigemTarget && m_fn_target_remove) {
        m_fn_target_remove(m_vigemClient, m_vigemTarget);
    }
    if (m_vigemTarget && m_fn_target_free) {
        m_fn_target_free(m_vigemTarget);
        m_vigemTarget = nullptr;
    }
    if (m_vigemClient && m_fn_disconnect && m_fn_free) {
        m_fn_disconnect(m_vigemClient);
        m_fn_free(m_vigemClient);
        m_vigemClient = nullptr;
    }
    if (m_hViGEmDll) {
        FreeLibrary(m_hViGEmDll);
        m_hViGEmDll = nullptr;
    }
}

bool Remapper::Start(HWND hwnd, LogCallback logCb, StatusCallback statusCb, InputCallback inputCb) {
    if (m_running.load()) return true;

    m_logCallback = std::move(logCb);
    m_statusCallback = std::move(statusCb);
    m_inputCallback = std::move(inputCb);

    if (!InitViGEm()) {
        return false;
    }

    m_running.store(true);
    m_workerThread = std::thread(&Remapper::WorkerLoop, this, hwnd);
    return true;
}

void Remapper::Stop() {
    if (!m_running.load()) return;

    m_running.store(false);
    if (m_workerThread.joinable()) {
        m_workerThread.join();
    }

    UninitViGEm();

    if (m_statusCallback) {
        m_statusCallback(RemapperStatus::Stopped, L"");
    }
}

void Remapper::WorkerLoop(HWND hwnd) {
    auto& loc = Localization::Instance();
    if (m_logCallback) m_logCallback(loc.Get(StringId::LogMapperStart));

    IDirectInput8W* directInput = nullptr;
    HRESULT hr = DirectInput8Create(GetModuleHandle(NULL), DIRECTINPUT_VERSION, IID_IDirectInput8W, (VOID**)&directInput, NULL);
    if (FAILED(hr) || !directInput) {
        if (m_logCallback) m_logCallback(L"ERROR: Failed to initialize DirectInput8.");
        if (m_statusCallback) m_statusCallback(RemapperStatus::Disconnected, L"");
        return;
    }

    while (m_running.load()) {
        if (m_statusCallback) m_statusCallback(RemapperStatus::Searching, L"");

        std::vector<DeviceChoice> devices;
        directInput->EnumDevices(DI8DEVCLASS_GAMECTRL, EnumDevicesCallback, &devices, DIEDFL_ATTACHEDONLY);

        if (devices.empty()) {
            // Sleep and retry auto-reconnect
            for (int i = 0; i < 20 && m_running.load(); ++i) {
                Sleep(100);
            }
            continue;
        }

        // Prioritize 8BitDo device
        DeviceChoice chosen = devices[0];
        for (const auto& dev : devices) {
            if (dev.is8BitDo) {
                chosen = dev;
                break;
            }
        }

        if (m_logCallback) m_logCallback(L"Target Device Found: " + chosen.name);

        LPDIRECTINPUTDEVICE8W joystick = nullptr;
        hr = directInput->CreateDevice(chosen.guid, &joystick, NULL);
        if (FAILED(hr) || !joystick) {
            Sleep(1000);
            continue;
        }

        hr = joystick->SetDataFormat(&c_dfDIJoystick2);
        if (FAILED(hr)) {
            joystick->Release();
            Sleep(1000);
            continue;
        }

        joystick->SetCooperativeLevel(hwnd, DISCL_BACKGROUND | DISCL_NONEXCLUSIVE);
        joystick->Acquire();

        if (m_logCallback) m_logCallback(loc.Get(StringId::LogMapperReady));
        if (m_statusCallback) m_statusCallback(RemapperStatus::Connected, chosen.name);

        // State cache for dirty checking
        XUSB_REPORT prevReport = {};
        int idleTicks = 0;

        while (m_running.load()) {
            hr = joystick->Poll();
            DIJOYSTATE2 state = {};
            hr = joystick->GetDeviceState(sizeof(DIJOYSTATE2), &state);

            if (FAILED(hr)) {
                // Device lost, attempt to reacquire
                hr = joystick->Acquire();
                if (FAILED(hr)) {
                    if (m_logCallback) m_logCallback(loc.Get(StringId::LogMapperDisconnected));
                    if (m_statusCallback) m_statusCallback(RemapperStatus::Disconnected, L"");
                    break;
                }
            }

            // Normalization & Dynamic Deadzone
            int curDz = m_deadzone.load();
            SHORT lx = ApplyDeadzone(NormalizeAxis(state.lX), curDz);
            SHORT ly = NegateAxis(ApplyDeadzone(NormalizeAxis(state.lY), curDz));
            SHORT rx = ApplyDeadzone(NormalizeAxis(state.lZ), curDz);
            SHORT ry = NegateAxis(ApplyDeadzone(NormalizeAxis(state.lRz), curDz));

            BYTE lt = state.rgbButtons[8] ? 255 : 0;
            BYTE rt = state.rgbButtons[9] ? 255 : 0;

            USHORT buttons = 0;
            if (state.rgbButtons[0])  buttons |= XUSB_GAMEPAD_A;
            if (state.rgbButtons[1])  buttons |= XUSB_GAMEPAD_B;
            if (state.rgbButtons[3])  buttons |= XUSB_GAMEPAD_X;
            if (state.rgbButtons[4])  buttons |= XUSB_GAMEPAD_Y;
            if (state.rgbButtons[6])  buttons |= XUSB_GAMEPAD_LEFT_SHOULDER;
            if (state.rgbButtons[7])  buttons |= XUSB_GAMEPAD_RIGHT_SHOULDER;
            if (state.rgbButtons[10]) buttons |= XUSB_GAMEPAD_BACK;
            if (state.rgbButtons[11]) buttons |= XUSB_GAMEPAD_START;
            if (state.rgbButtons[13]) buttons |= XUSB_GAMEPAD_LEFT_THUMB;
            if (state.rgbButtons[14]) buttons |= XUSB_GAMEPAD_RIGHT_THUMB;

            // D-Pad (POV)
            DWORD pov = state.rgdwPOV[0];
            if (LOWORD(pov) != 0xFFFF) {
                if (pov >= 31500 || pov <= 4500)   buttons |= XUSB_GAMEPAD_DPAD_UP;
                if (pov >= 4500 && pov <= 13500)   buttons |= XUSB_GAMEPAD_DPAD_RIGHT;
                if (pov >= 13500 && pov <= 22500)  buttons |= XUSB_GAMEPAD_DPAD_DOWN;
                if (pov >= 22500 && pov <= 31500)  buttons |= XUSB_GAMEPAD_DPAD_LEFT;
            }

            XUSB_REPORT report = {
                buttons,
                lt,
                rt,
                lx,
                ly,
                rx,
                ry
            };

            // Dirty checking
            bool changed = (memcmp(&report, &prevReport, sizeof(XUSB_REPORT)) != 0);
            if (!changed) {
                idleTicks++;
                if (idleTicks < KeepAliveTicks) {
                    Sleep(5);
                    continue;
                }
            } else if (m_inputCallback) {
                m_inputCallback(report);
            }

            idleTicks = 0;
            prevReport = report;

            if (m_vigemClient && m_vigemTarget && m_fn_target_update) {
                m_fn_target_update(m_vigemClient, m_vigemTarget, report);
            }

            Sleep(5);
        }

        joystick->Unacquire();
        joystick->Release();

        // 1 second backoff before retry scan
        for (int i = 0; i < 10 && m_running.load(); ++i) {
            Sleep(100);
        }
    }

    if (directInput) {
        directInput->Release();
    }
}

SHORT Remapper::NormalizeAxis(LONG v) {
    int centered = static_cast<int>(v) - 32767;
    if (centered < -32768) centered = -32768;
    if (centered > 32767) centered = 32767;
    return static_cast<SHORT>(centered);
}

SHORT Remapper::ApplyDeadzone(SHORT v, int dz) {
    if (dz <= 0) return v;
    if (v > -dz && v < dz) return 0;
    return v;
}

SHORT Remapper::NegateAxis(SHORT v) {
    if (v == -32768) return 32767;
    return -v;
}

} // namespace BitDoFixer
