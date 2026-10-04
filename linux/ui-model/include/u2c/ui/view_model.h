// The state behind the window. It is told what happens (service events, user actions) and answers what to show.
// Single threaded: the Qt layer calls it from the GUI thread.
#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "u2c/ui/display.h"
#include "u2c/ui/policy.h"
#include "u2c/ui/text_state.h"

namespace u2c::ui {

class ViewModel {
public:
    ViewModel(Catalog catalog, Settings settings, bool tray_available);

    void on_status(ServiceStatus status, const std::string& device_name);
    void on_battery(const BatteryInfo& battery);  // also runs the low battery alert
    void on_input(const PadReport& report);
    void on_log(const LogEvent& event, ClockTime now);  // formatted in the current language when it arrives
    void on_notice(const Notice& notice);
    void add_notice(UiNotice notice, const std::string& detail = std::string());  // from the UI itself (tray, Wayland)
    void poll(const ServiceState& state, int64_t now_ns);  // feeds the update rate readout

    StatusView status() const { return status_view(catalog_, status_, device_); }
    BatteryView battery() const { return battery_view(catalog_, battery_.level); }
    BadgeView badge() const { return mode_badge(settings_.nintendo_mode); }
    std::array<TileView, 4> tiles() const { return abxy_tiles(settings_.nintendo_mode, input_.buttons); }
    const PadReport& input() const { return input_; }
    std::string rate(int64_t now_ns) const { return rate_text(catalog_, rate_.view(now_ns)); }
    bool rate_visible() const { return status_ == ServiceStatus::Connected; }  // only while connected
    std::string tooltip() const { return tray_tooltip(catalog_, status_, battery_.level); }
    std::string log_text() const { return log_.text(); }
    std::string deadzone_label() const { return deadzone_text(catalog_, settings_.deadzone); }
    std::string polling_label() const { return polling_text(catalog_, settings_.polling_rate); }
    std::string curve_label() const { return curve_text(catalog_, settings_.response_curve); }
    ServiceStatus service_status() const { return status_; }
    int battery_level() const { return battery_.level; }
    bool tray_available() const { return tray_available_; }
    void set_tray_available(bool v) { tray_available_ = v; }

    bool take_battery_alert();  // true once: show the low battery balloon
    struct PendingNotice {
        UiNotice kind;
        std::string detail;
    };
    std::optional<PendingNotice> take_notice();  // each kind is shown once per run

    // user actions (the caller applies the returned settings to the service and saves them)
    const Settings& settings() const { return settings_; }
    const Settings& cycle_deadzone();
    const Settings& cycle_polling_rate();
    const Settings& cycle_curve();
    const Settings& set_checkbox(SettingId id, bool value);
    const Settings& set_language(const std::string& code);  // saved choice and active language
    // The language in use: the saved choice, or the one detected from the system.
    const std::string& active_language() const { return active_language_; }
    void set_active_language(std::string code) { active_language_ = std::move(code); }
    void set_catalog(Catalog catalog) { catalog_ = std::move(catalog); }
    void clear_log() { log_.clear(); }
    const Catalog& catalog() const { return catalog_; }

private:
    Catalog catalog_;
    Settings settings_;
    std::string active_language_ = "en";
    bool tray_available_;
    ServiceStatus status_ = ServiceStatus::Stopped;
    std::string device_;
    BatteryInfo battery_;
    PadReport input_;
    LogBuffer log_;
    RateReadout rate_;
    LowBatteryAlert alert_;
    bool alert_pending_ = false;
    std::vector<PendingNotice> notices_;
    uint32_t seen_notices_ = 0;
};

}  // namespace u2c::ui
