// What the window shows, decided without any toolkit: texts, colors and sizes. The Qt layer only draws them.
#pragma once
#include <array>
#include <cstdint>
#include <string>

#include "u2c/pad.h"
#include "u2c/service.h"
#include "u2c/translations.h"
#include "u2c/ui/layout.h"
#include "u2c/ui/palette.h"

namespace u2c::ui {

// Controller status card: dot color, status text and the name line ("No Device Detected" without a device).
struct StatusView {
    ColorId dot;
    std::string status_text;
    std::string name_text;
};
StatusView status_view(const Catalog& c, ServiceStatus status, const std::string& device_name);

// level < 0 means unknown
struct BatteryView {
    std::string percent_text;
    ColorId bar_color;
    int fill_percent;  // 0 when unknown or 0, else 1..100
    std::string status_text;  // low, moderate, healthy, or "No Device Detected"
    ColorId status_color;
};
BatteryView battery_view(const Catalog& c, int level);
// Width of the filled part of the bar for a given track width.
constexpr int battery_fill_width(int track_width, int fill_percent) { return track_width * fill_percent / 100; }

// "XBOX" / "NINTENDO" badge (brand names, never translated).
struct BadgeView {
    const char* text;
    ColorId color;
};
BadgeView mode_badge(bool nintendo_mode);

// The four ABXY tiles, left, top, bottom, right. `mask` is the bit of the mapped report that lights the tile,
// so the lit tile always matches its label (also in Nintendo Mode).
struct TileView {
    char label;
    uint16_t mask;
    bool lit;
};
std::array<TileView, 4> abxy_tiles(bool nintendo_mode, uint16_t buttons);
bool bumper_lit(uint16_t buttons, bool left);

struct Offset {
    int dx, dy;
};
Offset stick_dot_offset(int16_t x, int16_t y);  // pixels from the box center; the stick's y is up, so dy is inverted
int trigger_fill_height(uint8_t value);

// Tray tooltip: "Ultimate2CFixer: <status> (<battery>%)".
std::string tray_tooltip(const Catalog& c, ServiceStatus status, int battery_level);

// Text of the settings buttons ("Label: value").
std::string deadzone_text(const Catalog& c, int index);
std::string polling_text(const Catalog& c, int index);
std::string curve_text(const Catalog& c, int index);

// The live update rate readout: the measured value while the controller is active, the last value for 2 seconds
// after input stops, then "Idle". Never a made-up number.
class RateReadout {
public:
    struct View {
        bool idle = true;
        int hz = 0;
        float ms = 0.0f;
    };
    static constexpr int64_t kHoldNs = 2000000000LL;
    void update(int hz, float ms, int64_t now_ns);
    View view(int64_t now_ns) const;
    void reset() { have_ = false; }

private:
    bool have_ = false;
    int hz_ = 0;
    float ms_ = 0.0f;
    int64_t at_ns_ = 0;
};
std::string rate_text(const Catalog& c, const RateReadout::View& v);
std::string format_ms(float ms);

// Low battery alert: fires once when the level is 1..15 and the setting is on; armed again above 20.
class LowBatteryAlert {
public:
    bool update(int level, bool enabled);
private:
    bool sent_ = false;
};

}  // namespace u2c::ui
