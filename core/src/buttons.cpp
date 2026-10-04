#include "u2c/buttons.h"

namespace u2c {

int key_from_evdev_code(int code) {
    if (code < 0x130 || code > 0x13f) return -1;
    return code - 0x130;  // BTN_SOUTH .. 0x13f, same order as the Key enum
}

uint16_t map_buttons(uint32_t key_mask, bool nintendo_mode) {
    const uint16_t a = nintendo_mode ? kB : kA;
    const uint16_t b = nintendo_mode ? kA : kB;
    const uint16_t x = nintendo_mode ? kY : kX;
    const uint16_t y = nintendo_mode ? kX : kY;

    auto down = [key_mask](Key k) { return (key_mask & key_bit(k)) != 0; };
    uint16_t out = 0;
    if (down(kKeySouth)) out |= a;
    if (down(kKeyEast)) out |= b;
    if (down(kKeyNorth)) out |= x;
    if (down(kKeyWest)) out |= y;
    if (down(kKeyTl)) out |= kLeftShoulder;
    if (down(kKeyTr)) out |= kRightShoulder;
    if (down(kKeySelect)) out |= kBack;
    if (down(kKeyStart)) out |= kStart;
    if (down(kKeyThumbL)) out |= kLeftThumb;
    if (down(kKeyThumbR)) out |= kRightThumb;
    return out;
}

uint16_t dpad_from_hat(int hat_x, int hat_y) {
    uint16_t out = 0;
    if (hat_y < 0) out |= kDpadUp;
    if (hat_y > 0) out |= kDpadDown;
    if (hat_x < 0) out |= kDpadLeft;
    if (hat_x > 0) out |= kDpadRight;
    return out;
}

}  // namespace u2c
