#include "u2c/ui/display.h"

#include "u2c/axes.h"

#include <algorithm>
#include <cstdio>

namespace u2c::ui {

StatusView status_view(const Catalog& c, ServiceStatus status, const std::string& device_name) {
    StatusView v{ColorId::Gray, c.text("StatusStopped"), c.text("NoDevice")};
    switch (status) {
        case ServiceStatus::Connected:
            v.dot = ColorId::Green;
            v.status_text = c.text("StatusConnected");
            if (!device_name.empty()) v.name_text = device_name;
            break;
        case ServiceStatus::Searching:
            v.dot = ColorId::Amber;
            v.status_text = c.text("StatusSearching");
            break;
        case ServiceStatus::Disconnected:
            v.dot = ColorId::Red;
            v.status_text = c.text("WaitingReconnect");
            break;
        case ServiceStatus::Stopped:
            break;
    }
    return v;
}

BatteryView battery_view(const Catalog& c, int level) {
    BatteryView v;
    if (level >= 0) v.percent_text = std::to_string(level) + "%";
    else v.percent_text = "--%";
    v.fill_percent = level > 0 ? std::min(level, 100) : 0;
    v.bar_color = level <= 20 ? ColorId::Red : (level <= 50 ? ColorId::Amber : ColorId::Green);
    if (level < 0) {
        v.status_text = c.text("NoDevice");
        v.status_color = ColorId::TextMuted;
    } else if (level <= 20) {
        v.status_text = c.text("BatteryLow");
        v.status_color = ColorId::Red;
    } else if (level <= 50) {
        v.status_text = c.text("BatteryModerate");
        v.status_color = ColorId::TextSecondary;
    } else {
        v.status_text = c.text("BatteryHealthy");
        v.status_color = ColorId::TextSecondary;
    }
    return v;
}

BadgeView mode_badge(bool nintendo_mode) {
    return nintendo_mode ? BadgeView{"NINTENDO", ColorId::Amber} : BadgeView{"XBOX", ColorId::Green};
}

std::array<TileView, 4> abxy_tiles(bool nintendo_mode, uint16_t buttons) {
    // Xbox layout: X left, Y top, A bottom, B right. Nintendo layout: Y left, X top, B bottom, A right.
    // Each tile lights with the report bit that the physical button produces.
    std::array<TileView, 4> t;
    if (nintendo_mode) {
        t = {{{'Y', kX, false}, {'X', kY, false}, {'B', kA, false}, {'A', kB, false}}};
    } else {
        t = {{{'X', kX, false}, {'Y', kY, false}, {'A', kA, false}, {'B', kB, false}}};
    }
    for (auto& tile : t) tile.lit = (buttons & tile.mask) != 0;
    return t;
}

bool bumper_lit(uint16_t buttons, bool left) { return (buttons & (left ? kLeftShoulder : kRightShoulder)) != 0; }

Offset stick_dot_offset(int16_t x, int16_t y) {
    return {(x * kStickTravel) / 32768, -((y * kStickTravel) / 32768)};
}

int trigger_fill_height(uint8_t value) { return (value * kTriggerH) / 255; }

std::string tray_tooltip(const Catalog& c, ServiceStatus status, int battery_level) {
    std::string tip = c.text("AppTitle");
    if (status == ServiceStatus::Connected) {
        tip += ": " + c.text("StatusConnected");
        if (battery_level >= 0) tip += " (" + std::to_string(battery_level) + "%)";
    } else if (status == ServiceStatus::Searching) {
        tip += ": " + c.text("StatusSearching");
    } else if (status == ServiceStatus::Disconnected) {
        tip += ": " + c.text("WaitingReconnect");
    } else {
        tip += ": " + c.text("StatusStopped");
    }
    return tip;
}

std::string deadzone_text(const Catalog& c, int index) {
    static const char* const keys[4] = {"DeadzoneOff", "DeadzoneLow", "DeadzoneNormal", "DeadzoneHigh"};
    return c.text("DeadzoneLabel") + ": " + c.text(keys[std::clamp(index, 0, 3)]);
}

std::string polling_text(const Catalog& c, int index) {
    return c.text("PollingRateLabel") + ": " + std::to_string(kPollingRates[std::clamp(index, 0, 3)]) + " Hz";
}

std::string curve_text(const Catalog& c, int index) {
    static const char* const keys[3] = {"CurveLinear", "CurveSmooth", "CurveAggressive"};
    return c.text("CurveLabel") + ": " + c.text(keys[std::clamp(index, 0, 2)]);
}

void RateReadout::update(int hz, float ms, int64_t now_ns) {
    if (hz > 0) {
        have_ = true;
        hz_ = hz;
        ms_ = ms;
        at_ns_ = now_ns;
    }
}

RateReadout::View RateReadout::view(int64_t now_ns) const {
    View v;
    if (have_ && now_ns - at_ns_ < kHoldNs) {
        v.idle = false;
        v.hz = hz_;
        v.ms = ms_;
    }
    return v;
}

std::string format_ms(float ms) {
    const int tenths = static_cast<int>(ms * 10.0f + 0.5f);
    return std::to_string(tenths / 10) + "." + std::to_string(tenths % 10);
}

std::string rate_text(const Catalog& c, const RateReadout::View& v) {
    if (v.idle) return c.text("Idle");
    return c.format("LiveRate", {{"hz", std::to_string(v.hz)}, {"ms", format_ms(v.ms)}});
}

bool LowBatteryAlert::update(int level, bool enabled) {
    if (level > 0 && level <= 15 && !sent_ && enabled) {
        sent_ = true;
        return true;
    }
    if (level > 20) sent_ = false;
    return false;
}

}  // namespace u2c::ui
