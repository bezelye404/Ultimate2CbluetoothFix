#include "u2c/ui/text_state.h"

#include <algorithm>
#include <cctype>
#include <cstdio>

namespace u2c::ui {

std::string format_log_line(ClockTime t, const std::string& message) {
    char buf[16];
    std::snprintf(buf, sizeof buf, "[%02d:%02d:%02d] ", t.hour, t.minute, t.second);
    return buf + message;
}

void LogBuffer::push(ClockTime t, const std::string& message) {
    lines_.push_back(format_log_line(t, message));
    while (lines_.size() > kMaxLines) lines_.pop_front();
}

std::string LogBuffer::text() const {
    std::string out;
    for (size_t i = 0; i < lines_.size(); ++i) {
        if (i) out += '\n';
        out += lines_[i];
    }
    return out;
}

namespace {
bool contains(const std::vector<std::string>& list, const std::string& code) {
    return std::find(list.begin(), list.end(), code) != list.end();
}
}  // namespace

std::string detect_language(std::string_view locale, const std::vector<std::string>& available) {
    std::string lang;
    for (char c : locale) {
        if (c == '_' || c == '-' || c == '.' || c == '@') break;
        lang += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    if ((lang == "tr" || lang == "es") && contains(available, lang)) return lang;
    return "en";
}

std::string next_language(const std::string& current, const std::vector<std::string>& available) {
    if (available.empty()) return "en";
    const auto it = std::find(available.begin(), available.end(), current);
    if (it == available.end()) return available.front();
    const size_t next = static_cast<size_t>(it - available.begin() + 1) % available.size();
    return available[next];
}

std::string language_button_text(const std::string& current, const std::vector<std::string>& available) {
    std::string code = next_language(current, available);
    for (char& c : code) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return code;
}

std::string effective_language(const std::string& saved, std::string_view locale, const std::vector<std::string>& available) {
    if (!saved.empty() && contains(available, saved)) return saved;
    return detect_language(locale, available);
}

bool get_setting(const u2c::Settings& s, SettingId id) {
    switch (id) {
        case SettingId::StartWithSystem: return false;
        case SettingId::MinimizeOnClose: return s.minimize_on_close;
        case SettingId::AutoStartService: return s.auto_start_service;
        case SettingId::LowBatteryAlert: return s.low_battery_alert;
        case SettingId::NintendoMode: return s.nintendo_mode;
        case SettingId::HairTrigger: return s.hair_trigger;
        case SettingId::ExclusiveGrab: return s.exclusive_grab;
    }
    return false;
}

void set_setting(u2c::Settings& s, SettingId id, bool value) {
    switch (id) {
        case SettingId::StartWithSystem: break;
        case SettingId::MinimizeOnClose: s.minimize_on_close = value; break;
        case SettingId::AutoStartService: s.auto_start_service = value; break;
        case SettingId::LowBatteryAlert: s.low_battery_alert = value; break;
        case SettingId::NintendoMode: s.nintendo_mode = value; break;
        case SettingId::HairTrigger: s.hair_trigger = value; break;
        case SettingId::ExclusiveGrab: s.exclusive_grab = value; break;
    }
}

int next_deadzone(int index) { return (index + 1) % 4; }
int next_polling_rate(int index) { return (index + 1) % 4; }
int next_curve(int index) { return (index + 1) % 3; }

}  // namespace u2c::ui
