// Pure logic around evdev and the virtual pad, no system calls.
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

#include "u2c/pad.h"
#include "u2c/platform/backend.h"

namespace u2c::platform {

// Linux event codes used here (same numbers as <linux/input-event-codes.h>).
inline constexpr uint16_t kEvSyn = 0, kEvKey = 1, kEvAbs = 3;
inline constexpr uint16_t kSynReport = 0, kSynDropped = 3;
inline constexpr uint16_t kAbsX = 0x00, kAbsY = 0x01, kAbsZ = 0x02, kAbsRx = 0x03, kAbsRy = 0x04, kAbsRz = 0x05,
                          kAbsGas = 0x09, kAbsBrake = 0x0a, kAbsHat0X = 0x10, kAbsHat0Y = 0x11;

// The controller state built from events.
class RawState {
public:
    // Applies one event. Returns true for SYN_REPORT (a complete update: map and write now).
    bool apply(const InputEvent& e);
    // After SYN_DROPPED the caller must read the state again; true once, then cleared.
    bool take_dropped() { const bool d = dropped_; dropped_ = false; return d; }
    const RawInput& raw() const { return raw_; }
    void reset() { raw_ = RawInput{}; dropped_ = false; }

private:
    RawInput raw_;
    bool dropped_ = false;
};

// What the finder checks (measured on the Ultimate 2C in Bluetooth mode).
struct AbsInfo {
    bool present = false;
    int min = 0, max = 0;
};
inline constexpr uint16_t kVendor8BitDo = 0x2dc8;
// vendor is 8BitDo, axes X, Y, Z, RZ, GAS, BRAKE exist with range 0..255, the hat axes -1..1, and keys 0x130..0x13e exist.
bool matches_controller(uint16_t vendor, const std::array<AbsInfo, 0x12>& abs, bool (*has_key)(void*, uint16_t), void* key_ctx);

// Events that bring the virtual pad from `prev` to `next` (all values if prev is null), ending with SYN_REPORT.
// Layout of the kernel's xpad driver: Y axes inverted (up is negative), triggers on ABS_Z and ABS_RZ.
inline constexpr size_t kMaxPadEvents = 24;
size_t pad_events(const PadReport* prev, const PadReport& next, InputEvent (&out)[kMaxPadEvents]);

// Pad key codes in the order of the report bits.
struct PadButton {
    uint16_t xbox_bit;
    uint16_t key_code;
};
extern const PadButton kPadButtons[11];

}  // namespace u2c::platform
