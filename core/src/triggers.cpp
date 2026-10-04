#include "u2c/triggers.h"

namespace u2c {

uint8_t trigger_windows(int32_t axis16, int32_t idle16, bool click, bool hair_trigger) {
    if (click) return 255;
    int32_t diff = (axis16 >= idle16) ? (axis16 - idle16) : (idle16 - axis16);
    if (diff < 1500) return 0;
    if (hair_trigger) return 255;
    int32_t max_span = (idle16 <= 32768) ? (65535 - idle16) : idle16;
    if (max_span < 1000) max_span = 65535;
    int32_t scaled = (diff * 255) / max_span;
    if (scaled > 255) scaled = 255;
    if (scaled < 0) scaled = 0;
    return static_cast<uint8_t>(scaled);
}

uint8_t trigger_analog(int32_t axis16, int32_t idle16, bool click, bool hair_trigger, bool analog_present) {
    if (!analog_present) return click ? 255 : 0;
    return trigger_windows(axis16, idle16, false, hair_trigger);
}

}  // namespace u2c
