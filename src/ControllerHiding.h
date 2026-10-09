#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <cfgmgr32.h>
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
//
// What this cannot do: a program that already had the controller open (a browser tab, Steam) keeps it, and
// hiding or un-hiding produces no "device arrived" event for such programs. Only restarting the device
// (administrator) or switching the controller off and on does. So when the hiding is applied while the
// controller is connected, the user is told to switch the controller off and on once.
class ControllerHiding {
public:
    using LogFn = std::function<void(const std::wstring&, LogLevel)>;
    // Asks the window to tell the user something (a tray notification) about this message.
    using NoticeFn = std::function<void(StringId)>;

    explicit ControllerHiding(LogFn log, NoticeFn notice = nullptr);
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

    // The controller is connected but DirectInput cannot see it while hidden: give it back for a while
    // (the hiding resumes by itself afterwards).
    void Suspend(StringId reason, DWORD durationMs = 60000);
    bool IsSuspended() const { return GetTickCount64() < m_suspendedUntil.load(); }

private:
    void Apply(bool quietWhenNotFound);
    void Loop();
    void Log(StringId id);

    LogFn m_log;
    NoticeFn m_notice;
    DeviceHider m_hider;
    std::thread m_thread;
    std::atomic<bool> m_running{false};
    std::atomic<ULONGLONG> m_suspendedUntil{0};
    std::atomic<ULONGLONG> m_lastNoticeTick{0};
    std::atomic<DWORD> m_vid{0};
    std::atomic<DWORD> m_pid{0};
    std::atomic<int> m_lastReport{-1};
    std::atomic<unsigned> m_deviceChanges{0};   // bumped by Windows when a HID interface arrives or leaves
    HCMNOTIFICATION m_notify = nullptr;
};

} // namespace Ultimate2CFixer
