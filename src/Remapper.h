#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <string>
#include <functional>
#include <thread>
#include <atomic>
#include "Localization.h"
#include "../include/ViGEm/Client.h"
#include "DeviceHider.h"

namespace Ultimate2CFixer {

enum class RemapperStatus {
    Searching,
    Connected,
    Disconnected,
    Stopped
};

using LogCallback = std::function<void(const std::wstring&)>;
using StatusCallback = std::function<void(RemapperStatus, const std::wstring&)>;
using InputCallback = std::function<void(const XUSB_REPORT&)>;

class Remapper {
public:
    Remapper();
    ~Remapper();

    bool Start(HWND hwnd, LogCallback logCb, StatusCallback statusCb, InputCallback inputCb = nullptr);
    void Stop();
    bool IsRunning() const { return m_running.load(); }

    void SetDeadzone(int dz) { m_deadzone.store(dz); }
    int GetDeadzone() const { return m_deadzone.load(); }

    void SetNintendoMode(bool enable) { m_nintendoMode.store(enable); }
    bool GetNintendoMode() const { return m_nintendoMode.load(); }

    void SetHairTrigger(bool enable) { m_hairTrigger.store(enable); }
    bool GetHairTrigger() const { return m_hairTrigger.load(); }

    void SetPollingRate(int hz) { m_pollingRateHz.store(hz); }
    int GetPollingRate() const { return m_pollingRateHz.load(); }

    void SetResponseCurve(int curve) { m_responseCurve.store(curve); }
    int GetResponseCurve() const { return m_responseCurve.load(); }

    void SetHideRealDevice(bool enable) { m_hideReal.store(enable); }
    bool GetHideRealDevice() const { return m_hideReal.load(); }

    // Measured rate of controller updates (reports that changed the output), not the loop speed.
    int GetLiveHz() const { return m_liveHz.load(); }
    float GetLiveMs() const { return m_liveMs.load(); }
    // True until the first input, and again 2 seconds after the last one (the readout then shows "Idle").
    bool IsInputIdle() const {
        ULONGLONG last = m_lastInputTick.load();
        return last == 0 || GetTickCount64() - last > 2000;
    }

private:
    void WorkerLoop(HWND hwnd);
    bool InitViGEm();
    void UninitViGEm();
    void ApplyDeviceHiding(DWORD vid, DWORD pid);
    void NoteConnectFailure();
    void RevertHiding(StringId reason);

    static SHORT NormalizeAxis(LONG v);
    static SHORT ApplyDeadzone(SHORT v, int dz);
    static SHORT ApplyResponseCurve(SHORT v, int curveType);
    static SHORT NegateAxis(SHORT v);
    static BYTE CalculateTrigger(LONG axisVal, LONG idleVal, bool hairTrigger);

    std::atomic<bool> m_running{false};
    std::atomic<int> m_deadzone{4000};
    std::atomic<bool> m_nintendoMode{false};
    std::atomic<bool> m_hairTrigger{false};
    std::atomic<int> m_pollingRateHz{250};
    std::atomic<int> m_responseCurve{0};
    std::atomic<bool> m_hideReal{true};
    std::atomic<int> m_liveHz{250};
    std::atomic<float> m_liveMs{4.0f};
    std::atomic<ULONGLONG> m_lastInputTick{0};

    std::thread m_workerThread;
    LogCallback m_logCallback;
    StatusCallback m_statusCallback;
    InputCallback m_inputCallback;

    PVIGEM_CLIENT m_vigemClient{nullptr};
    PVIGEM_TARGET m_vigemTarget{nullptr};
    bool m_targetPlugged{false};

    LONG m_idleSlider0{0};
    LONG m_idleSlider1{0};
    LONG m_idleRx{0};
    LONG m_idleRy{0};

    // Worker-thread only state for hiding the physical controller.
    DeviceHider m_hider;
    int m_lastHideReport{-1};
    bool m_hidingSuspended{false};   // Hiding proved unsafe this session; stay visible.
    bool m_hideJustApplied{false};   // Hiding was applied but the device is not yet confirmed readable.
    int m_hideFailCount{0};
};

} // namespace Ultimate2CFixer
