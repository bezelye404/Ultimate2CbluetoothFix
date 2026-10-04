#include "u2c/ui/view_model.h"

namespace u2c::ui {

ViewModel::ViewModel(Catalog catalog, Settings settings, bool tray_available)
    : catalog_(std::move(catalog)), settings_(std::move(settings)), tray_available_(tray_available) {}

void ViewModel::on_status(ServiceStatus status, const std::string& device_name) {
    status_ = status;
    device_ = status == ServiceStatus::Connected ? device_name : std::string();
    if (status != ServiceStatus::Connected) {
        input_ = PadReport{};  // the live input is cleared when the controller is not connected
        rate_.reset();
    }
}

void ViewModel::on_battery(const BatteryInfo& battery) {
    battery_ = battery;
    if (alert_.update(battery.level, settings_.low_battery_alert)) alert_pending_ = true;
}

void ViewModel::on_input(const PadReport& report) { input_ = report; }

void ViewModel::on_log(const LogEvent& event, ClockTime now) {
    std::string text = catalog_.text(event.key);
    for (const auto& v : event.values) {
        const std::string token = "{" + v.first + "}";
        for (size_t pos = text.find(token); pos != std::string::npos; pos = text.find(token, pos + v.second.size()))
            text.replace(pos, token.size(), v.second);
    }
    log_.push(now, text);
}

void ViewModel::on_notice(const Notice& notice) { add_notice(ui_notice_from(notice.kind), notice.detail); }

void ViewModel::add_notice(UiNotice notice, const std::string& detail) {
    const uint32_t bit = 1u << static_cast<int>(notice);
    if (seen_notices_ & bit) return;       // each kind is shown only once per run
    seen_notices_ |= bit;
    notices_.push_back({notice, detail});
}

void ViewModel::poll(const ServiceState& state, int64_t now_ns) {
    if (state.status == ServiceStatus::Connected) rate_.update(state.live_hz, state.live_ms, now_ns);
}

bool ViewModel::take_battery_alert() {
    const bool v = alert_pending_;
    alert_pending_ = false;
    return v;
}

std::optional<ViewModel::PendingNotice> ViewModel::take_notice() {
    if (notices_.empty()) return std::nullopt;
    PendingNotice n = std::move(notices_.front());
    notices_.erase(notices_.begin());
    return n;
}

const Settings& ViewModel::cycle_deadzone() { settings_.deadzone = next_deadzone(settings_.deadzone); return settings_; }
const Settings& ViewModel::cycle_polling_rate() { settings_.polling_rate = next_polling_rate(settings_.polling_rate); return settings_; }
const Settings& ViewModel::cycle_curve() { settings_.response_curve = next_curve(settings_.response_curve); return settings_; }
const Settings& ViewModel::set_checkbox(SettingId id, bool value) { set_setting(settings_, id, value); return settings_; }
const Settings& ViewModel::set_language(const std::string& code) {
    settings_.language = code;
    active_language_ = code;
    return settings_;
}

}  // namespace u2c::ui
