#define DIRECTINPUT_VERSION 0x0800
#include "Remapper.h"
#include "Localization.h"
#include <dinput.h>
#include <vector>
#include <algorithm>
#include <cwctype>
#include <chrono>
#include <cmath>
#include <mmsystem.h>

#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "winmm.lib")

namespace Ultimate2CFixer {

namespace {
    constexpr int Deadzone = 4000;
    constexpr int KeepAliveTicks = 50; // ~250ms

    struct DeviceChoice {
        GUID guid;
        std::wstring name;
        std::wstring displayName;
        DWORD vid;
        DWORD pid;
        bool is8BitDo;
        bool isVirtual;
    };

    std::wstring ToLower(std::wstring str) {
        std::transform(str.begin(), str.end(), str.begin(), [](wchar_t c) { return (wchar_t)std::towlower(c); });
        return str;
    }

    BOOL CALLBACK EnumDevicesCallback(LPCDIDEVICEINSTANCEW lpddi, LPVOID pvRef) {
        auto* list = reinterpret_cast<std::vector<DeviceChoice>*>(pvRef);

        DWORD vid = LOWORD(lpddi->guidProduct.Data1);
        DWORD pid = HIWORD(lpddi->guidProduct.Data1);
        std::wstring instName = lpddi->tszInstanceName;
        std::wstring prodName = lpddi->tszProductName;
        std::wstring lowerInst = ToLower(instName);
        std::wstring lowerProd = ToLower(prodName);

        // Filter out virtual Xbox 360 controller (Microsoft VID 0x045E, PID 0x028E) to prevent loopback
        bool isVirtual = (vid == 0x045E && (pid == 0x028E || pid == 0x02A1 || pid == 0x028F)) ||
                         (lowerInst.find(L"vigem") != std::wstring::npos) ||
                         (lowerProd.find(L"vigem") != std::wstring::npos);

        // Detect 8BitDo hardware (Vendor ID 0x2DC8) or name
        bool is8BitDo = (vid == 0x2DC8) ||
                        (lowerInst.find(L"8bitdo") != std::wstring::npos) ||
                        (lowerProd.find(L"8bitdo") != std::wstring::npos);

        std::wstring displayName;
        if (is8BitDo) {
            if (lowerProd.find(L"ultimate") != std::wstring::npos || lowerProd.find(L"8bitdo") != std::wstring::npos) {
                displayName = prodName;
            } else if (lowerInst.find(L"8bitdo") != std::wstring::npos) {
                displayName = instName;
            } else {
                displayName = L"8BitDo Ultimate 2C Wireless";
            }
        } else {
            displayName = prodName.empty() ? instName : prodName;
        }

        list->push_back({ lpddi->guidInstance, instName, displayName, vid, pid, is8BitDo, isVirtual });
        return DIENUM_CONTINUE;
    }
}

Remapper::Remapper() = default;

Remapper::~Remapper() {
    Stop();
}

bool Remapper::InitViGEm() {
    m_vigemClient = vigem_alloc();
    if (!m_vigemClient) {
        if (m_logCallback) m_logCallback(L"ERROR: Could not allocate ViGEm client.");
        return false;
    }

    VIGEM_ERROR err = vigem_connect(m_vigemClient);
    if (!VIGEM_SUCCESS(err)) {
        if (m_logCallback) m_logCallback(L"ERROR: Could not connect to ViGEmBus driver.");
        vigem_free(m_vigemClient);
        m_vigemClient = nullptr;
        return false;
    }

    m_vigemTarget = vigem_target_x360_alloc();
    if (!m_vigemTarget) {
        vigem_disconnect(m_vigemClient);
        vigem_free(m_vigemClient);
        m_vigemClient = nullptr;
        return false;
    }

    m_targetPlugged = false;
    return true;
}

void Remapper::UninitViGEm() {
    if (m_vigemTarget) {
        if (m_vigemClient && m_targetPlugged) {
            vigem_target_remove(m_vigemClient, m_vigemTarget);
            m_targetPlugged = false;
        }
        vigem_target_free(m_vigemTarget);
        m_vigemTarget = nullptr;
    }
    if (m_vigemClient) {
        vigem_disconnect(m_vigemClient);
        vigem_free(m_vigemClient);
        m_vigemClient = nullptr;
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

    if (m_inputCallback) {
        XUSB_REPORT zeroReport = {};
        m_inputCallback(zeroReport);
    }

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

        std::vector<DeviceChoice> rawDevices;
        directInput->EnumDevices(DI8DEVCLASS_GAMECTRL, EnumDevicesCallback, &rawDevices, DIEDFL_ATTACHEDONLY);

        std::vector<DeviceChoice> devices;
        for (const auto& dev : rawDevices) {
            if (!dev.isVirtual) {
                devices.push_back(dev);
            }
        }

        // Strictly search for genuine 8BitDo controller
        const DeviceChoice* targetDevice = nullptr;
        for (const auto& dev : devices) {
            if (dev.is8BitDo) {
                targetDevice = &dev;
                break;
            }
        }

        if (!targetDevice) {
            if (m_targetPlugged && m_vigemClient && m_vigemTarget) {
                vigem_target_remove(m_vigemClient, m_vigemTarget);
                m_targetPlugged = false;
            }
            if (m_statusCallback) m_statusCallback(RemapperStatus::Searching, L"");
            for (int i = 0; i < 15 && m_running.load(); ++i) {
                Sleep(100);
            }
            continue;
        }

        DeviceChoice chosen = *targetDevice;

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
        hr = joystick->Acquire();
        if (FAILED(hr)) {
            joystick->Release();
            Sleep(500);
            continue;
        }

        // Verify genuine live communication before announcing connected
        joystick->Poll();
        DIJOYSTATE2 testState = {};
        hr = joystick->GetDeviceState(sizeof(DIJOYSTATE2), &testState);
        if (FAILED(hr)) {
            joystick->Unacquire();
            joystick->Release();
            Sleep(500);
            continue;
        }

        // Cache idle resting positions of trigger axes (Z Axis = LT, Z Rotation = RT)
        m_idleZ = testState.lZ;
        m_idleRz = testState.lRz;
        m_idleRx = testState.lRx;
        m_idleRy = testState.lRy;

        // Plug in virtual target on-demand once physical controller is verified alive
        if (!m_targetPlugged && m_vigemClient && m_vigemTarget) {
            VIGEM_ERROR plugErr = vigem_target_add(m_vigemClient, m_vigemTarget);
            if (VIGEM_SUCCESS(plugErr)) {
                m_targetPlugged = true;
            }
        }

        if (m_logCallback) m_logCallback(chosen.displayName + L" connected.");
        if (m_statusCallback) m_statusCallback(RemapperStatus::Connected, chosen.displayName);

        // State cache for dirty checking
        XUSB_REPORT prevReport = {};
        int idleTicks = 0;

        timeBeginPeriod(1);
        auto lastHzTime = std::chrono::steady_clock::now();
        int pollCount = 0;

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
                    if (m_targetPlugged && m_vigemClient && m_vigemTarget) {
                        vigem_target_remove(m_vigemClient, m_vigemTarget);
                        m_targetPlugged = false;
                    }
                    if (m_inputCallback) {
                        XUSB_REPORT zeroReport = {};
                        m_inputCallback(zeroReport);
                    }
                    break;
                }
            }

            // Normalization, Deadzone & Response Curve
            int curDz = m_deadzone.load();
            int curve = m_responseCurve.load();

            // Left Stick: lX and lY
            SHORT lx = ApplyResponseCurve(ApplyDeadzone(NormalizeAxis(state.lX), curDz), curve);
            SHORT ly = NegateAxis(ApplyResponseCurve(ApplyDeadzone(NormalizeAxis(state.lY), curDz), curve));

            // Right Stick: lRx (X-Rotation) and lRy (Y-Rotation)
            SHORT rx = ApplyResponseCurve(ApplyDeadzone(NormalizeAxis(state.lRx), curDz), curve);
            SHORT ry = NegateAxis(ApplyResponseCurve(ApplyDeadzone(NormalizeAxis(state.lRy), curDz), curve));

            // Triggers: lZ (Z-Axis = Left Trigger) and lRz (Z-Rotation = Right Trigger)
            bool hair = m_hairTrigger.load();
            BYTE lt = CalculateTrigger(state.lZ, m_idleZ, state.rgbButtons[8] != 0, hair);
            BYTE rt = CalculateTrigger(state.lRz, m_idleRz, state.rgbButtons[9] != 0, hair);

            bool nintendoMode = m_nintendoMode.load();
            USHORT btnA = static_cast<USHORT>(nintendoMode ? XUSB_GAMEPAD_B : XUSB_GAMEPAD_A);
            USHORT btnB = static_cast<USHORT>(nintendoMode ? XUSB_GAMEPAD_A : XUSB_GAMEPAD_B);
            USHORT btnX = static_cast<USHORT>(nintendoMode ? XUSB_GAMEPAD_Y : XUSB_GAMEPAD_X);
            USHORT btnY = static_cast<USHORT>(nintendoMode ? XUSB_GAMEPAD_X : XUSB_GAMEPAD_Y);

            USHORT buttons = 0;
            if (state.rgbButtons[0])  buttons |= btnA;
            if (state.rgbButtons[1])  buttons |= btnB;
            if (state.rgbButtons[3])  buttons |= btnX;
            if (state.rgbButtons[4])  buttons |= btnY;
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

            pollCount++;
            auto now = std::chrono::steady_clock::now();
            auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastHzTime).count();
            if (elapsedMs >= 1000) {
                int currentHz = static_cast<int>((pollCount * 1000.0f) / elapsedMs);
                float currentMs = currentHz > 0 ? (1000.0f / currentHz) : 0.0f;
                m_liveHz.store(currentHz);
                m_liveMs.store(currentMs);
                pollCount = 0;
                lastHzTime = now;
            }

            int targetHz = m_pollingRateHz.load();
            DWORD sleepMs = (targetHz >= 1000) ? 1 : (targetHz >= 500 ? 2 : (targetHz >= 250 ? 4 : 8));

            // Dirty checking
            bool changed = (memcmp(&report, &prevReport, sizeof(XUSB_REPORT)) != 0);
            if (!changed) {
                idleTicks++;
                if (idleTicks < KeepAliveTicks) {
                    Sleep(sleepMs);
                    continue;
                }
            } else if (m_inputCallback) {
                m_inputCallback(report);
            }

            idleTicks = 0;
            prevReport = report;

            if (m_vigemClient && m_vigemTarget) {
                vigem_target_x360_update(m_vigemClient, m_vigemTarget, report);
            }

            Sleep(sleepMs);
        }

        timeEndPeriod(1);

        if (m_targetPlugged && m_vigemClient && m_vigemTarget) {
            vigem_target_remove(m_vigemClient, m_vigemTarget);
            m_targetPlugged = false;
        }

        if (m_inputCallback) {
            XUSB_REPORT zeroReport = {};
            m_inputCallback(zeroReport);
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

SHORT Remapper::ApplyResponseCurve(SHORT v, int curveType) {
    if (curveType == 0 || v == 0) return v; // 0 = Linear

    float norm = static_cast<float>(v) / 32767.0f;
    float sign = (norm >= 0.0f) ? 1.0f : -1.0f;
    float absNorm = fabsf(norm);
    if (absNorm > 1.0f) absNorm = 1.0f;

    float resultNorm = absNorm;
    if (curveType == 1) {
        // Smooth Aim (S-curve): gentle around deadzone, rapid at outer bounds
        resultNorm = (absNorm * absNorm * (3.0f - 2.0f * absNorm));
    } else if (curveType == 2) {
        // Aggressive: fast snap
        resultNorm = sqrtf(absNorm);
    }

    int res = static_cast<int>(sign * resultNorm * 32767.0f);
    if (res > 32767) res = 32767;
    if (res < -32768) res = -32768;
    return static_cast<SHORT>(res);
}

BYTE Remapper::CalculateTrigger(LONG axisVal, LONG idleVal, bool btnPressed, bool hairTrigger) {
    LONG diff = (axisVal >= idleVal) ? (axisVal - idleVal) : (idleVal - axisVal);
    if (diff < 1500) {
        return btnPressed ? 255 : 0;
    }
    if (hairTrigger) {
        if (diff > 2500 || btnPressed) {
            return 255;
        }
    }
    LONG maxSpan = (idleVal <= 32768) ? (65535 - idleVal) : idleVal;
    if (maxSpan < 1000) maxSpan = 65535;
    LONG scaled = (diff * 255) / maxSpan;
    if (scaled > 255) scaled = 255;
    if (scaled < 0) scaled = 0;
    if (btnPressed) {
        return 255;
    }
    return static_cast<BYTE>(scaled);
}

} // namespace Ultimate2CFixer
