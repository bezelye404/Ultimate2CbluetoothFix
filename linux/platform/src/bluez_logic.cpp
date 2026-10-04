#include "u2c/platform/bluez_logic.h"

#include <algorithm>
#include <cctype>

namespace u2c::platform {

namespace {
std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}
}  // namespace

bool is_8bitdo_device(const BluezDevice& d) {
    return lower(d.modalias).find("v2dc8") != std::string::npos || lower(d.name).find("8bitdo") != std::string::npos;
}

std::optional<BatteryInfo> pick_battery(std::vector<BluezDevice> devices) {
    std::sort(devices.begin(), devices.end(), [](const BluezDevice& a, const BluezDevice& b) { return a.path < b.path; });
    for (const BluezDevice& d : devices) {
        if (!d.connected || !d.has_battery || d.percentage < 0 || d.percentage > 100) continue;
        if (!is_8bitdo_device(d)) continue;
        return BatteryInfo{d.percentage, d.name.empty() ? std::string("8BitDo controller") : d.name};
    }
    return std::nullopt;
}

}  // namespace u2c::platform
