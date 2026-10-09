#define DIRECTINPUT_VERSION 0x0800
#include "Remapper.h"
#include "Localization.h"
#include "PadFormat.h"
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

    // One reading of the controller, independent of the DirectInput data format in use.
    struct PadSample {
        LONG lX, lY, lZ, lRz;       // sticks
        LONG lt, rt;                // analog trigger depth
        LONG ltAlt, rtAlt;          // second analog source (only the legacy format has one)
        DWORD pov;
        BYTE buttons[16];
    };

    HRESULT ReadPad(LPDIRECTINPUTDEVICE8W dev, bool usageFormat, PadSample& out) {
        if (usageFormat) {
            PadRaw raw = {};
            HRESULT hr = dev->GetDeviceState(sizeof(raw), &raw);
            if (FAILED(hr)) return hr;
            out.lX = raw.lX; out.lY = raw.lY; out.lZ = raw.lZ; out.lRz = raw.lRz;
            out.lt = raw.brake; out.rt = raw.gas;
            out.ltAlt = 0; out.rtAlt = 0;
            out.pov = raw.pov;
            memcpy(out.buttons, raw.buttons, sizeof(out.buttons));
            return S_OK;
        }
        DIJOYSTATE2 st = {};
        HRESULT hr = dev->GetDeviceState(sizeof(st), &st);
        if (FAILED(hr)) return hr;
        out.lX = st.lX; out.lY = st.lY; out.lZ = st.lZ; out.lRz = st.lRz;
        out.lt = st.rglSlider[0]; out.rt = st.rglSlider[1];
        out.ltAlt = st.lRx; out.rtAlt = st.lRy;
        out.pov = st.rgdwPOV[0];
        memcpy(out.buttons, st.rgbButtons, sizeof(out.buttons));
        return S_OK;
    }

    BOOL CALLBACK CountSlidersCallback(LPCDIDEVICEOBJECTINSTANCEW obj, LPVOID pvRef) {
        if (IsEqualGUID(obj->guidType, GUID_Slider)) {
            ++*reinterpret_cast<int*>(pvRef);
        }
        return DIENUM_CONTINUE;
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
    auto reportUnavailable = [this]() {
        if (m_logCallback) m_logCallback(Localization::Instance().Get(StringId::LogViGEmUnavailable));
    };

    m_vigemClient = vigem_alloc();
    if (!m_vigemClient) {
        reportUnavailable();
        return false;
    }

    VIGEM_ERROR err = vigem_connect(m_vigemClient);
    if (!VIGEM_SUCCESS(err)) {
        reportUnavailable();
        vigem_free(m_vigemClient);
        m_vigemClient = nullptr;
        return false;
    }

    m_vigemTarget = vigem_target_x360_alloc();
    if (!m_vigemTarget) {
        reportUnavailable();
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

    m_hideJustApplied = false;
    m_hideFailCount = 0;

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
        if (m_logCallback) m_logCallback(loc.Get(StringId::LogInputSystemFailed));
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
            // Hiding must never lock this app out: when the controller is connected but DirectInput cannot see
            // it, the hiding is the likely cause (NoteConnectFailure then undoes it after a few tries).
            const DWORD vid = m_hiding ? m_hiding->Vid() : 0;
            if (m_hiding && m_hiding->IsHidden() && vid != 0 && DeviceHider::IsPresent(vid, m_hiding->Pid())) {
                m_hideJustApplied = true;
                NoteConnectFailure();
            } else {
                m_hideFailCount = 0;
            }
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

        if (m_hiding) {
            m_hiding->SetController(chosen.vid, chosen.pid);
            m_hiding->HideNow();
        }
        // From here on a failure to open the controller is blamed on the hiding (see NoteConnectFailure).
        m_hideJustApplied = (m_hiding != nullptr && m_hiding->IsHidden());

        LPDIRECTINPUTDEVICE8W joystick = nullptr;
        hr = directInput->CreateDevice(chosen.guid, &joystick, NULL);
        if (FAILED(hr) || !joystick) {
            NoteConnectFailure();
            Sleep(1000);
            continue;
        }

        // Preferred: map every control by HID usage so the analog triggers (Brake/Accelerator) are readable.
        // Fallback: the standard DIJOYSTATE2 layout.
        const bool usageFormat = SetPadDataFormat(joystick);
        hr = usageFormat ? S_OK : joystick->SetDataFormat(&c_dfDIJoystick2);
        if (FAILED(hr)) {
            joystick->Release();
            NoteConnectFailure();
            Sleep(1000);
            continue;
        }

        joystick->SetCooperativeLevel(hwnd, DISCL_BACKGROUND | DISCL_NONEXCLUSIVE);
        hr = joystick->Acquire();
        if (FAILED(hr)) {
            joystick->Release();
            NoteConnectFailure();
            Sleep(500);
            continue;
        }

        // Verify genuine live communication before announcing connected
        joystick->Poll();
        PadSample testState = {};
        hr = ReadPad(joystick, usageFormat, testState);
        if (FAILED(hr)) {
            joystick->Unacquire();
            joystick->Release();
            NoteConnectFailure();
            Sleep(500);
            continue;
        }

        // The controller is readable, so hiding did not lock this application out.
        m_hideJustApplied = false;
        m_hideFailCount = 0;

        // Before the first real report arrives DirectInput answers with neutral placeholders (32767 on every
        // axis, triggers included). Let a few reports come in so the resting positions below are real.
        for (int settle = 0; settle < 3 && m_running.load(); ++settle) {
            Sleep(20);
            joystick->Poll();
            ReadPad(joystick, usageFormat, testState);
        }

        // Resting positions of the trigger axes. The Brake/Accelerator usages are 0 when released (their HID
        // range starts at 0, same as on Linux); only the legacy layout needs a measured resting value.
        m_idleSlider0 = usageFormat ? 0 : testState.lt;
        m_idleSlider1 = usageFormat ? 0 : testState.rt;
        m_idleRx = usageFormat ? 0 : testState.ltAlt;
        m_idleRy = usageFormat ? 0 : testState.rtAlt;

        // The analog trigger depth is used whenever the controller provides it (the Brake/Accelerator axes
        // of the usage format, or two real sliders in the legacy format). Only without analog axes does the
        // digital click stand in for the trigger (same rule as the Linux version).
        bool analogTriggers = usageFormat;
        if (!analogTriggers) {
            int sliderCount = 0;
            joystick->EnumObjects(CountSlidersCallback, &sliderCount, DIDFT_AXIS);
            analogTriggers = (sliderCount >= 2);
        }

        // Plug in virtual target on-demand once physical controller is verified alive
        if (!m_targetPlugged && m_vigemClient && m_vigemTarget) {
            VIGEM_ERROR plugErr = vigem_target_add(m_vigemClient, m_vigemTarget);
            if (VIGEM_SUCCESS(plugErr)) {
                m_targetPlugged = true;
            }
        }

        if (m_logCallback) {
            wchar_t connectedMsg[256];
            swprintf_s(connectedMsg, loc.Get(StringId::LogControllerConnected).c_str(), chosen.displayName.c_str());
            m_logCallback(connectedMsg);
        }
        m_lastInputTick.store(0);
        m_liveHz.store(0);
        m_liveMs.store(0.0f);
        if (m_statusCallback) m_statusCallback(RemapperStatus::Connected, chosen.displayName);

        // State cache for dirty checking
        XUSB_REPORT prevReport = {};
        int idleTicks = 0;

        timeBeginPeriod(1);
        auto lastHzTime = std::chrono::steady_clock::now();
        int changeCount = 0;

        while (m_running.load()) {
            hr = joystick->Poll();
            PadSample state = {};
            hr = ReadPad(joystick, usageFormat, state);

            if (FAILED(hr)) {
                // Device lost, attempt to reacquire
                hr = joystick->Acquire();
                if (SUCCEEDED(hr)) {
                    // Nothing valid was read this pass; do not turn an empty sample into stick input.
                    Sleep(1);
                    continue;
                }
                {
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

            // Right Stick: lZ (horizontal) and lRz (vertical)
            SHORT rx = ApplyResponseCurve(ApplyDeadzone(NormalizeAxis(state.lZ), curDz), curve);
            SHORT ry = NegateAxis(ApplyResponseCurve(ApplyDeadzone(NormalizeAxis(state.lRz), curDz), curve));

            // Triggers: analog depth (Brake = LT, Accelerator = RT); the digital click (buttons 8 and 9) is only a fallback
            bool hair = m_hairTrigger.load();
            BYTE lt, rt;
            if (analogTriggers) {
                // The click fires early in the pull and must not force the full value.
                lt = (std::max)(CalculateTrigger(state.lt, m_idleSlider0, hair),
                                CalculateTrigger(state.ltAlt, m_idleRx, hair));
                rt = (std::max)(CalculateTrigger(state.rt, m_idleSlider1, hair),
                                CalculateTrigger(state.rtAlt, m_idleRy, hair));
            } else {
                lt = (state.buttons[8] != 0) ? 255 : 0;
                rt = (state.buttons[9] != 0) ? 255 : 0;
            }

            bool nintendoMode = m_nintendoMode.load();
            USHORT btnA = static_cast<USHORT>(nintendoMode ? XUSB_GAMEPAD_B : XUSB_GAMEPAD_A);
            USHORT btnB = static_cast<USHORT>(nintendoMode ? XUSB_GAMEPAD_A : XUSB_GAMEPAD_B);
            USHORT btnX = static_cast<USHORT>(nintendoMode ? XUSB_GAMEPAD_Y : XUSB_GAMEPAD_X);
            USHORT btnY = static_cast<USHORT>(nintendoMode ? XUSB_GAMEPAD_X : XUSB_GAMEPAD_Y);

            USHORT buttons = 0;
            if (state.buttons[0])  buttons |= btnA;
            if (state.buttons[1])  buttons |= btnB;
            if (state.buttons[3])  buttons |= btnX;
            if (state.buttons[4])  buttons |= btnY;
            if (state.buttons[6])  buttons |= XUSB_GAMEPAD_LEFT_SHOULDER;
            if (state.buttons[7])  buttons |= XUSB_GAMEPAD_RIGHT_SHOULDER;
            if (state.buttons[10]) buttons |= XUSB_GAMEPAD_BACK;
            if (state.buttons[11]) buttons |= XUSB_GAMEPAD_START;
            if (state.buttons[13]) buttons |= XUSB_GAMEPAD_LEFT_THUMB;
            if (state.buttons[14]) buttons |= XUSB_GAMEPAD_RIGHT_THUMB;

            // D-Pad (POV)
            DWORD pov = state.pov;
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

            // The readout shows how often the controller really updates, so only count passes that changed the output.
            const bool changed = (memcmp(&report, &prevReport, sizeof(XUSB_REPORT)) != 0);
            if (changed) {
                ++changeCount;
                m_lastInputTick.store(GetTickCount64());
            }

            auto now = std::chrono::steady_clock::now();
            auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastHzTime).count();
            if (elapsedMs >= 1000) {
                if (changeCount > 0) {
                    int currentHz = static_cast<int>((changeCount * 1000.0f) / elapsedMs);
                    float currentMs = currentHz > 0 ? (1000.0f / currentHz) : 0.0f;
                    m_liveHz.store(currentHz);
                    m_liveMs.store(currentMs);
                }
                changeCount = 0;
                lastHzTime = now;
            }

            int targetHz = m_pollingRateHz.load();
            DWORD sleepMs = (targetHz >= 1000) ? 1 : (targetHz >= 500 ? 2 : (targetHz >= 250 ? 4 : 8));

            // Dirty checking
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

// If the controller cannot be opened shortly after hiding it, undo the hiding so the
// user is never left without a working controller.
void Remapper::NoteConnectFailure() {
    if (!m_hideJustApplied) return;
    if (++m_hideFailCount >= 3) {
        RevertHiding(StringId::LogHideSuspended);
    }
}

void Remapper::RevertHiding(StringId reason) {
    if (m_hiding) m_hiding->Suspend(reason);
    m_hideJustApplied = false;
    m_hideFailCount = 0;
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

BYTE Remapper::CalculateTrigger(LONG axisVal, LONG idleVal, bool hairTrigger) {
    LONG diff = (axisVal >= idleVal) ? (axisVal - idleVal) : (idleVal - axisVal);
    if (diff < 1500) {
        return 0;
    }
    if (hairTrigger) {
        return 255;
    }
    LONG maxSpan = (idleVal <= 32768) ? (65535 - idleVal) : idleVal;
    if (maxSpan < 1000) maxSpan = 65535;
    LONG scaled = (diff * 255) / maxSpan;
    if (scaled > 255) scaled = 255;
    if (scaled < 0) scaled = 0;
    return static_cast<BYTE>(scaled);
}

} // namespace Ultimate2CFixer
