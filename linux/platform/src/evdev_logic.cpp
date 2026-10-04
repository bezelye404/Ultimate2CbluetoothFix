#include "u2c/platform/evdev_logic.h"

#include <algorithm>

#include "u2c/buttons.h"

namespace u2c::platform {

namespace {
int clamp(int v, int lo, int hi) { return std::max(lo, std::min(hi, v)); }
}  // namespace

bool RawState::apply(const InputEvent& e) {
    if (e.type == kEvSyn) {
        if (e.code == kSynReport) return true;
        if (e.code == kSynDropped) dropped_ = true;
        return false;
    }
    if (e.type == kEvAbs) {
        switch (e.code) {
            case kAbsX: raw_.abs_x = clamp(e.value, 0, 255); break;
            case kAbsY: raw_.abs_y = clamp(e.value, 0, 255); break;
            case kAbsZ: raw_.abs_z = clamp(e.value, 0, 255); break;
            case kAbsRz: raw_.abs_rz = clamp(e.value, 0, 255); break;
            case kAbsGas: raw_.abs_gas = clamp(e.value, 0, 255); break;
            case kAbsBrake: raw_.abs_brake = clamp(e.value, 0, 255); break;
            case kAbsHat0X: raw_.hat_x = clamp(e.value, -1, 1); break;
            case kAbsHat0Y: raw_.hat_y = clamp(e.value, -1, 1); break;
            default: break;
        }
        return false;
    }
    if (e.type == kEvKey) {
        const int key = key_from_evdev_code(e.code);
        if (key < 0) return false;
        if (e.value == 0) raw_.keys &= ~key_bit(static_cast<Key>(key));
        else if (e.value == 1) raw_.keys |= key_bit(static_cast<Key>(key));
        // value 2 is auto repeat: no change
    }
    return false;
}

bool matches_controller(uint16_t vendor, const std::array<AbsInfo, 0x12>& abs, bool (*has_key)(void*, uint16_t), void* key_ctx) {
    if (vendor != kVendor8BitDo) return false;
    for (uint16_t code : {kAbsX, kAbsY, kAbsZ, kAbsRz, kAbsGas, kAbsBrake}) {
        if (!abs[code].present || abs[code].min != 0 || abs[code].max != 255) return false;
    }
    for (uint16_t code : {kAbsHat0X, kAbsHat0Y}) {
        if (!abs[code].present || abs[code].min != -1 || abs[code].max != 1) return false;
    }
    for (uint16_t key = 0x130; key <= 0x13e; ++key)
        if (!has_key(key_ctx, key)) return false;
    return true;
}

const PadButton kPadButtons[11] = {
    {kA, 0x130},
    {kB, 0x131},
    {kX, 0x133},
    {kY, 0x134},
    {kLeftShoulder, 0x136},
    {kRightShoulder, 0x137},
    {kBack, 0x13a},
    {kStart, 0x13b},
    {kGuide, 0x13c},
    {kLeftThumb, 0x13d},
    {kRightThumb, 0x13e},
};

size_t pad_events(const PadReport* prev, const PadReport& next, InputEvent (&out)[kMaxPadEvents]) {
    size_t n = 0;
    auto add = [&](uint16_t type, uint16_t code, int value) { out[n++] = InputEvent{type, code, value}; };
    auto abs_changed = [&](auto field) { return prev == nullptr || prev->*field != next.*field; };

    if (abs_changed(&PadReport::lx)) add(kEvAbs, kAbsX, next.lx);
    if (abs_changed(&PadReport::ly)) add(kEvAbs, kAbsY, static_cast<int16_t>(~next.ly));
    if (abs_changed(&PadReport::rx)) add(kEvAbs, kAbsRx, next.rx);
    if (abs_changed(&PadReport::ry)) add(kEvAbs, kAbsRy, static_cast<int16_t>(~next.ry));
    if (abs_changed(&PadReport::left_trigger)) add(kEvAbs, kAbsZ, next.left_trigger);
    if (abs_changed(&PadReport::right_trigger)) add(kEvAbs, kAbsRz, next.right_trigger);

    const uint16_t pb = prev ? prev->buttons : 0;
    const uint16_t nb = next.buttons;
    auto hat_x = [](uint16_t b) { return ((b & kDpadRight) ? 1 : 0) - ((b & kDpadLeft) ? 1 : 0); };
    auto hat_y = [](uint16_t b) { return ((b & kDpadDown) ? 1 : 0) - ((b & kDpadUp) ? 1 : 0); };
    if (prev == nullptr || hat_x(pb) != hat_x(nb)) add(kEvAbs, kAbsHat0X, hat_x(nb));
    if (prev == nullptr || hat_y(pb) != hat_y(nb)) add(kEvAbs, kAbsHat0Y, hat_y(nb));
    for (const PadButton& b : kPadButtons)
        if (prev == nullptr || ((pb ^ nb) & b.xbox_bit)) add(kEvKey, b.key_code, (nb & b.xbox_bit) ? 1 : 0);
    add(kEvSyn, kSynReport, 0);
    return n;
}

}  // namespace u2c::platform
