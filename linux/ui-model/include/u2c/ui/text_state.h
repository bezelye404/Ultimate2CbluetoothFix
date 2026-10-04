// Log box, language handling and the settings screen definition. Pure logic.
#pragma once
#include <deque>
#include <string>
#include <string_view>
#include <vector>

#include "u2c/settings.h"

namespace u2c::ui {

// Translation keys the window itself uses (a test checks that every language file has them).
inline constexpr const char* kUiKeys[] = {
    "AppTitle", "StatusTitle", "BatteryTitle", "StartBtn", "StopBtn", "LogsTitle", "ClearBtn", "SettingsTitle", "SettingsBack",
    "LiveInputTitle", "TrayOpen", "TrayExit", "LowBatteryAlertTitle", "LowBatteryAlertMsg", "NoDevice", "Idle",
    "StatusConnected", "StatusSearching", "StatusStopped", "WaitingReconnect", "BatteryLow", "BatteryModerate", "BatteryHealthy",
    "MinimizeOnClose", "StartWithSystem", "AutoStartService", "LowBatteryNotification", "NintendoMode", "HairTrigger",
    "ExclusiveGrab", "ExclusiveGrabHelp", "PollingRateLabel", "CurveLabel", "CurveLinear", "CurveSmooth", "CurveAggressive",
    "DeadzoneLabel", "DeadzoneOff", "DeadzoneLow", "DeadzoneNormal", "DeadzoneHigh", "LiveRate", "LogAutostartFailed",
};

struct ClockTime {
    int hour = 0, minute = 0, second = 0;
};
std::string format_log_line(ClockTime t, const std::string& message);

// The log box keeps the last 100 lines; older ones drop out.
class LogBuffer {
public:
    static constexpr size_t kMaxLines = 100;
    void push(ClockTime t, const std::string& message);
    void clear() { lines_.clear(); }
    size_t size() const { return lines_.size(); }
    std::string text() const;  // all lines joined with "\n", no trailing newline
private:
    std::deque<std::string> lines_;
};

// Language codes in the order of the language button.
inline constexpr const char* kAllLanguages[3] = {"en", "tr", "es"};

// From a locale name such as "tr_TR.UTF-8", "es_419", "en_US" or "C": "tr" or "es" if available, else "en".
std::string detect_language(std::string_view locale, const std::vector<std::string>& available);
// The language after `current` in the list (wraps around). An unknown current starts from the first.
std::string next_language(const std::string& current, const std::vector<std::string>& available);
// The button shows the code of the language it will switch to ("TR" while English is active).
std::string language_button_text(const std::string& current, const std::vector<std::string>& available);
// The saved language if available, else the one detected from the locale.
std::string effective_language(const std::string& saved, std::string_view locale, const std::vector<std::string>& available);

// Settings screen: the checkboxes in screen order. `key` is the translation key of the label.
enum class SettingId { StartWithSystem, MinimizeOnClose, AutoStartService, LowBatteryAlert, NintendoMode, HairTrigger, ExclusiveGrab };
struct CheckboxSpec {
    SettingId id;
    const char* key;
};
inline constexpr CheckboxSpec kCheckboxes[7] = {
    {SettingId::StartWithSystem, "StartWithSystem"},
    {SettingId::MinimizeOnClose, "MinimizeOnClose"},
    {SettingId::AutoStartService, "AutoStartService"},
    {SettingId::LowBatteryAlert, "LowBatteryNotification"},
    {SettingId::NintendoMode, "NintendoMode"},
    {SettingId::HairTrigger, "HairTrigger"},
    {SettingId::ExclusiveGrab, "ExclusiveGrab"},
};
// StartWithSystem is not stored in the settings file (the autostart entry is the truth), so get returns false
// and set does nothing for it.
bool get_setting(const u2c::Settings& s, SettingId id);
void set_setting(u2c::Settings& s, SettingId id, bool value);

int next_deadzone(int index);
int next_polling_rate(int index);
int next_curve(int index);

}  // namespace u2c::ui
