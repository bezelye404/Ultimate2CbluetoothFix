#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

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
class DeviceHider {
public:
    static bool IsAvailable();

    // Removes leftovers of a previous session that ended unexpectedly.
    // Returns true when something was restored.
    static bool RecoverStale();

    HideResult Hide(DWORD vid, DWORD pid);

    // Returns true when entries added by this application were removed.
    bool Restore();

    bool IsHidden() const { return m_hidden; }

private:
    bool m_hidden{false};
};

} // namespace Ultimate2CFixer
