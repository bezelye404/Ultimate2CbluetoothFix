#include "u2c/ui/demo.h"

#include <algorithm>
#include <cmath>

#include "u2c/buttons.h"

namespace u2c::ui {

namespace {
int axis(double v) { return std::clamp(static_cast<int>(std::lround(127.5 + 127.5 * v)), 0, 255); }
}  // namespace

RawInput demo_input(double t) {
    RawInput in;
    in.abs_x = axis(std::cos(t * 1.3));
    in.abs_y = axis(std::sin(t * 1.7));
    in.abs_z = axis(std::sin(t * 0.9));
    in.abs_rz = axis(std::cos(t * 1.1));
    in.abs_brake = axis(std::sin(t * 0.8)) / 2;
    in.abs_gas = axis(std::cos(t * 0.6)) / 2;
    const int step = static_cast<int>(std::floor(std::max(t, 0.0) * 1.5)) % 12;  // changes every 2/3 of a second
    const Key keys[] = {kKeySouth, kKeyEast, kKeyNorth, kKeyWest, kKeyTl, kKeyTr};
    if (step < 6) in.keys |= key_bit(keys[step]);
    if (step >= 6 && step < 10) {
        const int hat = step - 6;
        in.hat_y = hat == 0 ? -1 : (hat == 2 ? 1 : 0);
        in.hat_x = hat == 1 ? 1 : (hat == 3 ? -1 : 0);
    }
    return in;
}

}  // namespace u2c::ui
