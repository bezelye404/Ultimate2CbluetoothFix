// Stick axis pipeline. Pure functions, no allocation, no OS calls.
// The formulas match the Windows version bit for bit, including its 32-bit float curve math.
#pragma once
#include <cstdint>

namespace u2c {

// The controller reports 0..255, the formulas work on the 16-bit range 0..65535. Scaling by 257 maps 0 to 0 and 255 to 65535.
constexpr int32_t scale_8_to_16(int v8) { return static_cast<int32_t>(v8) * 257; }

int16_t normalize_axis(int32_t v16);  // v - 32767, clamped to int16
int16_t apply_deadzone(int16_t v, int deadzone);  // square dead zone, no rescaling
int16_t apply_curve(int16_t v, int curve);  // 0 linear, 1 smooth aim (smoothstep), 2 aggressive (square root)
int16_t negate_axis(int16_t v);  // -32768 becomes 32767

// Whole pipeline for one stick axis. Y axes are negated after the curve.
int16_t stick_axis(int32_t raw16, int deadzone, int curve);
int16_t stick_axis_inverted(int32_t raw16, int deadzone, int curve);

// Preset tables (index to value).
constexpr int kDeadzonePresets[4] = {0, 2600, 4000, 6500};
constexpr int kPollingRates[4] = {125, 250, 500, 1000};

}  // namespace u2c
