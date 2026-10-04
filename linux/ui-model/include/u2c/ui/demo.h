// Scripted controller movement for the demo mode: sticks on circles, pulsing triggers, cycling buttons and D-pad.
#pragma once
#include "u2c/pad.h"

namespace u2c::ui {

RawInput demo_input(double seconds);

}  // namespace u2c::ui
