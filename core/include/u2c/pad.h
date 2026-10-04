// Raw controller state to the virtual Xbox 360 report.
#pragma once
#include <cstdint>

#include "u2c/buttons.h"

namespace u2c {

// Raw state as evdev reports it (axes 0..255).
struct RawInput {
    int abs_x = 127, abs_y = 127;
    int abs_z = 127, abs_rz = 127;  // right stick (horizontal, vertical)
    int abs_brake = 0, abs_gas = 0;  // left and right trigger
    int hat_x = 0, hat_y = 0;
    uint32_t keys = 0;
    bool analog_triggers_present = true;
};

struct MapConfig {
    int deadzone = 4000;  // raw value of the preset, Windows default (Normal)
    int curve = 0;
    bool hair_trigger = false;
    bool nintendo_mode = false;
};

struct PadReport {
    uint16_t buttons = 0;
    uint8_t left_trigger = 0, right_trigger = 0;
    int16_t lx = 0, ly = 0, rx = 0, ry = 0;
    friend bool operator==(const PadReport&, const PadReport&) = default;
};

// The triggers rest at the axis minimum on this controller.
constexpr int32_t kTriggerIdle16 = 0;

PadReport map_input(const RawInput& in, const MapConfig& cfg);

}  // namespace u2c
