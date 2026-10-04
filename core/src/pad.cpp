#include "u2c/pad.h"

#include "u2c/axes.h"
#include "u2c/triggers.h"

namespace u2c {

PadReport map_input(const RawInput& in, const MapConfig& cfg) {
    PadReport r;
    r.lx = stick_axis(scale_8_to_16(in.abs_x), cfg.deadzone, cfg.curve);
    r.ly = stick_axis_inverted(scale_8_to_16(in.abs_y), cfg.deadzone, cfg.curve);
    r.rx = stick_axis(scale_8_to_16(in.abs_z), cfg.deadzone, cfg.curve);
    r.ry = stick_axis_inverted(scale_8_to_16(in.abs_rz), cfg.deadzone, cfg.curve);

    const bool lt_click = (in.keys & key_bit(kKeyTl2)) != 0;
    const bool rt_click = (in.keys & key_bit(kKeyTr2)) != 0;
    r.left_trigger = trigger_analog(scale_8_to_16(in.abs_brake), kTriggerIdle16, lt_click, cfg.hair_trigger, in.analog_triggers_present);
    r.right_trigger = trigger_analog(scale_8_to_16(in.abs_gas), kTriggerIdle16, rt_click, cfg.hair_trigger, in.analog_triggers_present);

    r.buttons = static_cast<uint16_t>(map_buttons(in.keys, cfg.nintendo_mode) | dpad_from_hat(in.hat_x, in.hat_y));
    return r;
}

}  // namespace u2c
