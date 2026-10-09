#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <atomic>
#include <functional>
#include <string>
#include <thread>
#include "DeviceHider.h"
#include "Localization.h"

namespace Ultimate2CFixer {

// Keeps the physical controller hidden from other programs for as long as the application is open.
//
// It is owned by the application, not by the controller service: stopping and starting the service must not
// give the controller back, because a program that sees it in that gap (Steam, a game) keeps it open and the
// game then receives every input twice (the real controller and the virtual pad). HidHide only blocks
// programs that open the controller after it was hidden.
//
// A watchdog thread hides the remembered controller before it connects, and puts the hiding back within half
// a second when another program removes it or switches HidHide off. The controller is given back in Stop().
class ControllerHiding {
public:
    using LogFn = std::function<void(const std::wstring&)>;

    explicit ControllerHiding(LogFn log);
    ~ControllerHiding();

    void Start();
    void Stop();

    // The controller service found the controller; remember its USB id for the next session.
    void SetController(DWORD vid, DWORD pid);
    DWORD Vid() const { return m_vid.load(); }
    DWORD Pid() const { return m_pid.load(); }

    // Applies the hiding right now (the service calls this before it opens the controller).
    void HideNow();

    bool IsHidden() const { return m_hider.IsHidden(); }

    // The controller could not be read while hidden: give it back for the rest of this session.
    void Suspend(StringId reason);

private:
    void Apply(bool quietWhenNotFound);
    void Loop();
    void Log(StringId id);

    LogFn m_log;
    DeviceHider m_hider;
    std::thread m_thread;
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_suspended{false};
    std::atomic<DWORD> m_vid{0};
    std::atomic<DWORD> m_pid{0};
    std::atomic<int> m_lastReport{-1};
};

} // namespace Ultimate2CFixer
