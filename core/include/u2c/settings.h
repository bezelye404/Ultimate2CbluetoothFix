// Settings model and text file format. No file access here: the platform layer reads and writes the file.
// Format: one "Key=Value" per line, "#" or ";" starts a comment, blank lines are ignored.
// Keys are the Windows registry value names (case-insensitive). Booleans are 0/1 (true/false also work),
// choices are the index of the preset.
// A broken file never fails: bad lines and values become warnings and the default is used.
#pragma once
#include <string>
#include <string_view>
#include <vector>

#include "u2c/pad.h"

namespace u2c {

struct Settings {
    bool minimize_on_close = true;
    bool auto_start_service = true;
    bool low_battery_alert = true;
    bool nintendo_mode = false;
    bool hair_trigger = false;
    bool exclusive_grab = true;  // Linux only: hide the real controller from other programs while connected
    int deadzone = 2;  // index into kDeadzonePresets (0%, 8%, 12%, 20%)
    int polling_rate = 1;  // index into kPollingRates (125, 250, 500, 1000 Hz)
    int response_curve = 0;  // 0 linear, 1 smooth aim, 2 aggressive
    std::string language;  // "en", "tr", "es"; empty means use the system language
    friend bool operator==(const Settings&, const Settings&) = default;
};

struct SettingsWarning {
    enum class Reason { MalformedLine, UnknownKey, InvalidValue };
    int line = 0;
    std::string key;  // as written in the file, empty for a malformed line
    Reason reason = Reason::MalformedLine;
};

struct ParseResult {
    Settings settings;
    std::vector<SettingsWarning> warnings;
};

// Never fails. For duplicate keys the last one wins.
ParseResult parse_settings(std::string_view text);

// `existing` is the current file content (may be empty). Comments, blank lines and unknown keys in it are kept,
// known keys are updated in place, missing keys are appended.
// An empty `existing` gets a short header comment.
std::string serialize_settings(const Settings& s, std::string_view existing);

MapConfig to_map_config(const Settings& s);
int polling_hz(const Settings& s);

constexpr const char* kSupportedLanguages[3] = {"en", "tr", "es"};

}  // namespace u2c
