// Which Bluetooth device is the controller. No D-Bus here, so it is testable.
#pragma once
#include <optional>
#include <string>
#include <vector>

#include "u2c/service.h"

namespace u2c::platform {

// What the reader collects about one Bluetooth device (org.bluez.Device1 and org.bluez.Battery1).
struct BluezDevice {
    std::string path;
    std::string name;
    std::string modalias;  // for example "usb:v2DC8p301Bd0001"
    bool connected = false;
    bool has_battery = false;
    int percentage = -1;
};

// 8BitDo hardware: vendor 2DC8 in the modalias, or "8bitdo" in the name.
bool is_8bitdo_device(const BluezDevice& d);

// The battery to show: the first connected 8BitDo device (by object path) that reports 0..100.
std::optional<BatteryInfo> pick_battery(std::vector<BluezDevice> devices);

}  // namespace u2c::platform
