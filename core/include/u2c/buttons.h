// Linux evdev key codes to Xbox 360 button bits, Nintendo Mode swap, D-pad from the hat axes.
#pragma once
#include <cstdint>

namespace u2c {

// Same bit values as XUSB_BUTTON, so the report matches a real Xbox 360 pad.
enum XboxButton : uint16_t {
    kDpadUp = 0x0001, kDpadDown = 0x0002, kDpadLeft = 0x0004, kDpadRight = 0x0008,
    kStart = 0x0010, kBack = 0x0020, kLeftThumb = 0x0040, kRightThumb = 0x0080,
    kLeftShoulder = 0x0100, kRightShoulder = 0x0200, kGuide = 0x0400,
    kA = 0x1000, kB = 0x2000, kX = 0x4000, kY = 0x8000,
};

// The keys this controller reports, as bit positions of a key mask.
enum Key : int {
    kKeySouth, kKeyEast, kKeyC, kKeyNorth, kKeyWest, kKeyZ, kKeyTl, kKeyTr, kKeyTl2, kKeyTr2,
    kKeySelect, kKeyStart, kKeyMode, kKeyThumbL, kKeyThumbR, kKeyCount
};

// evdev key code (BTN_SOUTH = 0x130 ...) to Key, or -1 for an unknown code.
int key_from_evdev_code(int code);
constexpr uint32_t key_bit(Key k) { return uint32_t{1} << static_cast<int>(k); }

// Report buttons without D-pad and triggers: South, East, North, West, Tl, Tr, Select, Start, ThumbL, ThumbR.
// Tl2 and Tr2 are only trigger clicks; Mode, C and Z are not mapped.
uint16_t map_buttons(uint32_t key_mask, bool nintendo_mode);

// hat_y < 0 is up, hat_x < 0 is left
uint16_t dpad_from_hat(int hat_x, int hat_y);

}  // namespace u2c
