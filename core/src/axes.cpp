#include "u2c/axes.h"

#include <cmath>

namespace u2c {

int16_t normalize_axis(int32_t v16) {
    int centered = static_cast<int>(v16) - 32767;
    if (centered < -32768) centered = -32768;
    if (centered > 32767) centered = 32767;
    return static_cast<int16_t>(centered);
}

int16_t apply_deadzone(int16_t v, int deadzone) {
    if (deadzone <= 0) return v;
    if (v > -deadzone && v < deadzone) return 0;
    return v;
}

int16_t negate_axis(int16_t v) {
    if (v == -32768) return 32767;
    return static_cast<int16_t>(-v);
}

int16_t apply_curve(int16_t v, int curve) {
    if (curve == 0 || v == 0) return v;

    float norm = static_cast<float>(v) / 32767.0f;
    float sign = (norm >= 0.0f) ? 1.0f : -1.0f;
    float abs_norm = std::fabs(norm);
    if (abs_norm > 1.0f) abs_norm = 1.0f;

    float result = abs_norm;
    if (curve == 1) {
        result = (abs_norm * abs_norm * (3.0f - 2.0f * abs_norm));
    } else if (curve == 2) {
        result = std::sqrt(abs_norm);
    }

    int res = static_cast<int>(sign * result * 32767.0f);
    if (res > 32767) res = 32767;
    if (res < -32768) res = -32768;
    return static_cast<int16_t>(res);
}

int16_t stick_axis(int32_t raw16, int deadzone, int curve) {
    return apply_curve(apply_deadzone(normalize_axis(raw16), deadzone), curve);
}

int16_t stick_axis_inverted(int32_t raw16, int deadzone, int curve) {
    return negate_axis(stick_axis(raw16, deadzone, curve));
}

}  // namespace u2c
