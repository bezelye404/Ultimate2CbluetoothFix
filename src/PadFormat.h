#pragma once

#ifndef DIRECTINPUT_VERSION
#define DIRECTINPUT_VERSION 0x0800
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <dinput.h>

namespace Ultimate2CFixer {

// Data layout produced by SetPadDataFormat().
//
// The 8BitDo Ultimate 2C reports its analog triggers as the HID usages Brake (page 0x02, 0xC5) and
// Accelerator (page 0x02, 0xC4). DirectInput exposes them as extra axes of the same type as the sticks,
// and the standard DIJOYSTATE2 layout has no slot for them (its slider slots only accept real sliders),
// so they would always read 0. This layout maps every control by its HID usage instead.
struct PadRaw {
    LONG lX, lY;      // left stick
    LONG lZ, lRz;     // right stick
    LONG brake;       // left trigger (analog)
    LONG gas;         // right trigger (analog)
    DWORD pov;        // D-pad
    BYTE buttons[16];
};

// Applies the usage based data format. Returns false when the device does not expose all six axes this
// way (the caller then falls back to DIJOYSTATE2).
bool SetPadDataFormat(IDirectInputDevice8W* device);

} // namespace Ultimate2CFixer
