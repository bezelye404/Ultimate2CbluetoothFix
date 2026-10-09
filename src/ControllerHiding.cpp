#include "ControllerHiding.h"

namespace Ultimate2CFixer {

ControllerHiding::ControllerHiding(LogFn log, NoticeFn notice) : m_log(std::move(log)), m_notice(std::move(notice)) {}

ControllerHiding::~ControllerHiding() {
    Stop();
}

void ControllerHiding::Log(StringId id) {
    if (m_log) m_log(Localization::Instance().Get(id), LevelOf(id));
}

void ControllerHiding::Start() {
    if (m_running.exchange(true)) return;

    // The controller seen in an earlier session can be hidden before it connects this time.
    DWORD vid = 0, pid = 0;
    if (DeviceHider::LoadLastController(vid, pid)) {
        m_vid = vid;
        m_pid = pid;
    } else if (DeviceHider::RecoverStale()) {
        // Leftover hiding of a session that ended unexpectedly, and no controller to hide again: give it back.
        // (With a known controller the leftover is taken over by Hide() instead, without any gap.)
        Log(StringId::LogHideRecovered);
    }
    m_suspendedUntil = 0;
    m_lastReport = -1;

    // Windows tells us when a HID interface appears or disappears (a new identity of the controller), so the
    // watchdog only has to search for the controller's entries then, not twice a second.
    CM_NOTIFY_FILTER filter = {};
    filter.cbSize = sizeof(filter);
    filter.FilterType = CM_NOTIFY_FILTER_TYPE_DEVICEINTERFACE;
    filter.u.DeviceInterface.ClassGuid = { 0x4D1E55B2, 0xF16F, 0x11CF, { 0x88, 0xCB, 0x00, 0x11, 0x11, 0x00, 0x00, 0x30 } };   // GUID_DEVINTERFACE_HID
    auto onChange = [](HCMNOTIFICATION, PVOID ctx, CM_NOTIFY_ACTION, PCM_NOTIFY_EVENT_DATA, DWORD) -> DWORD {
        static_cast<std::atomic<unsigned>*>(ctx)->fetch_add(1);
        return ERROR_SUCCESS;
    };
    if (CM_Register_Notification(&filter, &m_deviceChanges, onChange, &m_notify) != CR_SUCCESS) m_notify = nullptr;

    m_thread = std::thread(&ControllerHiding::Loop, this);
}

void ControllerHiding::Stop() {
    if (!m_running.exchange(false)) return;
    if (m_thread.joinable()) m_thread.join();
    if (m_notify) {
        CM_Unregister_Notification(m_notify);
        m_notify = nullptr;
    }
    if (m_hider.Restore()) Log(StringId::LogHideRestored);
}

void ControllerHiding::SetController(DWORD vid, DWORD pid) {
    if (vid == m_vid.load() && pid == m_pid.load()) return;
    m_vid = vid;
    m_pid = pid;
    DeviceHider::SaveLastController(vid, pid);
}

void ControllerHiding::HideNow() {
    if (!IsSuspended()) Apply(false);
}

void ControllerHiding::Suspend(StringId reason, DWORD durationMs) {
    m_suspendedUntil = GetTickCount64() + durationMs;
    m_lastReport = -1;
    m_hider.Restore();
    Log(reason);
}

void ControllerHiding::Apply(bool quietWhenNotFound) {
    const DWORD vid = m_vid.load();
    if (vid == 0) return;

    const DWORD pid = m_pid.load();
    const bool connected = DeviceHider::IsPresent(vid, pid);   // checked before hiding, which changes nothing about it
    HideResult result = m_hider.Hide(vid, pid);
    if (result == HideResult::StillHidden) return;
    if (result == HideResult::Repaired) {
        Log(StringId::LogHideRepaired);
        return;
    }
    if (result == HideResult::DeviceNotFound && quietWhenNotFound) return;

    // Hiding something that is connected and may already be in use: programs holding it will not notice.
    if (result == HideResult::Hidden && connected) {
        const ULONGLONG now = GetTickCount64();
        const ULONGLONG last = m_lastNoticeTick.load();
        if (last == 0 || now - last > 10 * 60 * 1000) {
            m_lastNoticeTick = now;
            Log(StringId::LogHideNeedsReconnect);
            if (m_notice) m_notice(StringId::LogHideNeedsReconnect);
        }
    }

    int code = static_cast<int>(result);
    if (m_lastReport.exchange(code) == code) return;   // report every outcome once

    StringId msg = StringId::LogHideFailed;
    switch (result) {
        case HideResult::Hidden:           msg = StringId::LogHideActive; break;
        case HideResult::Adopted:          msg = StringId::LogHideActive; break;
        case HideResult::StillHidden:
        case HideResult::Repaired:         return;   // handled above
        case HideResult::AlreadyHidden:    msg = StringId::LogHideAlready; break;
        case HideResult::NotInstalled:     msg = StringId::LogHideMissing; break;
        case HideResult::DeviceNotFound:   msg = StringId::LogHideNoDevice; break;
        case HideResult::UnsupportedSetup: msg = StringId::LogHideCustomSetup; break;
        case HideResult::Failed:           msg = StringId::LogHideFailed; break;
    }
    Log(msg);
}

void ControllerHiding::Loop() {
    unsigned seenChanges = m_deviceChanges.load();
    ULONGLONG lastScan = 0;
    while (m_running.load()) {
        if (!IsSuspended() && m_vid.load() != 0) {
            if (m_hider.IsHidden()) {
                // Cheap check every pass (three small driver reads); the search for the controller's entries only
                // when Windows reported a device change, or every 10 s when no notification is available.
                const unsigned changes = m_deviceChanges.load();
                const ULONGLONG now = GetTickCount64();
                const bool rescan = changes != seenChanges || now - lastScan >= (m_notify ? 30000u : 10000u);
                if (rescan) {
                    seenChanges = changes;
                    lastScan = now;
                }
                if (m_hider.Maintain(rescan)) Log(StringId::LogHideRepaired);
            } else {
                Apply(true);
            }
        }
        for (int i = 0; i < 5 && m_running.load(); ++i) {
            Sleep(100);
        }
    }
}

} // namespace Ultimate2CFixer
