// Battery level from BlueZ (org.bluez.Battery1) through sd-bus. Follows D-Bus signals, no polling.
// Returns nullptr when built without libsystemd.
#pragma once
#include <memory>

#include "u2c/platform/backend.h"

namespace u2c::platform {

std::unique_ptr<BatteryProvider> make_bluez_battery_provider();

}  // namespace u2c::platform
