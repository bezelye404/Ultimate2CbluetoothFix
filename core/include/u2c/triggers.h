// Trigger value (0..255) from an analog axis.
#pragma once
#include <cstdint>

namespace u2c {

// Copy of the Windows behavior: a pressed digital click forces 255, which makes the analog range unusable on the
// Ultimate 2C. Kept for tests and comparison.
uint8_t trigger_windows(int32_t axis16, int32_t idle16, bool click, bool hair_trigger);

// Linux behavior: the analog axis drives the value and the click does not force 255. Same threshold and scaling
// as the Windows formula. Without an analog axis the click is the fallback (255 when pressed).
uint8_t trigger_analog(int32_t axis16, int32_t idle16, bool click, bool hair_trigger, bool analog_present);

}  // namespace u2c
