#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <string>
#include <functional>
#include <thread>
#include <atomic>
#include "../include/ViGEm/Client.h"

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

private:
    void WorkerLoop(HWND hwnd);
    bool InitViGEm();
    void UninitViGEm();

    static SHORT NormalizeAxis(LONG v);
    static SHORT ApplyDeadzone(SHORT v, int dz);
    static SHORT NegateAxis(SHORT v);

    std::atomic<bool> m_running{false};
    std::atomic<int> m_deadzone{4000};
    std::atomic<bool> m_nintendoMode{false};
    std::thread m_workerThread;
    LogCallback m_logCallback;
    StatusCallback m_statusCallback;
    InputCallback m_inputCallback;

    PVIGEM_CLIENT m_vigemClient{nullptr};
    PVIGEM_TARGET m_vigemTarget{nullptr};
};

} // namespace Ultimate2CFixer
