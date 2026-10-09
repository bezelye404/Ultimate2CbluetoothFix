#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <atomic>
#include <mutex>

namespace Ultimate2CFixer {

enum class HideResult {
    Hidden,           // Entries were added and hiding is now active.
    AlreadyHidden,    // The user's own setup already hides the device; nothing was changed.
    NotInstalled,     // The hiding driver is not present.
    DeviceNotFound,   // The physical controller could not be located.
    UnsupportedSetup, // The user's existing configuration is one we must not touch.
    Failed
};

// Hides the physical controller from other applications so games only see the
// virtual gamepad. Only the entries added by this class are ever removed again;
// the user's own hiding configuration is never overwritten.
//
// HidHide only blocks programs that open the device AFTER it was hidden; a program
// that already holds the controller open keeps it. That is why the hiding is applied
// as early as possible (also to the remembered, currently absent device entries) and
// is kept in place by Maintain().
//
// All methods may be called from different threads.
class DeviceHider {
public:
    static bool IsAvailable();

    // Removes leftovers of a previous session that ended unexpectedly.
    // Returns true when something was restored.
    static bool RecoverStale();

    // True when a controller with this USB id is currently connected.
    static bool IsPresent(DWORD vid, DWORD pid);

    // The controller seen last, so its entries can be hidden before it connects again.
    static void SaveLastController(DWORD vid, DWORD pid);
    static bool LoadLastController(DWORD& vid, DWORD& pid);

    HideResult Hide(DWORD vid, DWORD pid);

    // Verifies that the hiding is still in place and puts back whatever another program
    // removed or switched off. Returns true when something had to be repaired.
    bool Maintain();

    // Returns true when entries added by this application were removed.
    bool Restore();

    bool IsHidden() const { return m_hidden.load(); }

private:
    bool MaintainLocked();

    std::mutex m_lock;
    std::atomic<bool> m_hidden{false};
    DWORD m_vid{0};
    DWORD m_pid{0};
};

} // namespace Ultimate2CFixer
