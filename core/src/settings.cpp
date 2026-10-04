#include "u2c/settings.h"

#include <array>
#include <cctype>

#include "u2c/axes.h"

namespace u2c {

namespace {

enum class Kind { Bool, Index, Language };
struct KeyInfo {
    const char* name;
    Kind kind;
    int max_index;
};

// Order of the keys in a new file.
constexpr std::array<KeyInfo, 10> kKeys = {{
    {"MinimizeOnClose", Kind::Bool, 0},
    {"AutoStart", Kind::Bool, 0},
    {"LowBatteryAlert", Kind::Bool, 0},
    {"NintendoMode", Kind::Bool, 0},
    {"HairTrigger", Kind::Bool, 0},
    {"ExclusiveGrab", Kind::Bool, 0},
    {"Deadzone", Kind::Index, 3},
    {"PollingRate", Kind::Index, 3},
    {"ResponseCurve", Kind::Index, 2},
    {"Language", Kind::Language, 0},
}};

char lower(char c) { return static_cast<char>(std::tolower(static_cast<unsigned char>(c))); }

bool equals_nocase(std::string_view a, std::string_view b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i)
        if (lower(a[i]) != lower(b[i])) return false;
    return true;
}

std::string_view trim(std::string_view s) {
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) s.remove_prefix(1);
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r')) s.remove_suffix(1);
    return s;
}

int find_key(std::string_view key) {
    for (size_t i = 0; i < kKeys.size(); ++i)
        if (equals_nocase(key, kKeys[i].name)) return static_cast<int>(i);
    return -1;
}

bool parse_bool(std::string_view v, bool& out) {
    if (v == "1" || equals_nocase(v, "true")) { out = true; return true; }
    if (v == "0" || equals_nocase(v, "false")) { out = false; return true; }
    return false;
}

bool parse_index(std::string_view v, int max_index, int& out) {
    if (v.empty() || v.size() > 3) return false;
    int n = 0;
    for (char c : v) {
        if (c < '0' || c > '9') return false;
        n = n * 10 + (c - '0');
    }
    if (n > max_index) return false;
    out = n;
    return true;
}

// An empty value means no saved choice and is valid.
bool parse_language(std::string_view v, std::string& out) {
    if (v.empty()) { out.clear(); return true; }
    for (const char* lang : kSupportedLanguages)
        if (equals_nocase(v, lang)) { out = lang; return true; }
    return false;
}

std::string value_text(const Settings& s, int key_index) {
    switch (key_index) {
        case 0: return s.minimize_on_close ? "1" : "0";
        case 1: return s.auto_start_service ? "1" : "0";
        case 2: return s.low_battery_alert ? "1" : "0";
        case 3: return s.nintendo_mode ? "1" : "0";
        case 4: return s.hair_trigger ? "1" : "0";
        case 5: return s.exclusive_grab ? "1" : "0";
        case 6: return std::to_string(s.deadzone);
        case 7: return std::to_string(s.polling_rate);
        case 8: return std::to_string(s.response_curve);
        default: return s.language;
    }
}

// "\r" is removed later by trim().
template <typename Fn>
void for_each_line(std::string_view text, Fn fn) {
    if (text.size() >= 3 && text.substr(0, 3) == "\xEF\xBB\xBF") text.remove_prefix(3);  // UTF-8 byte order mark
    int line_no = 0;
    while (!text.empty()) {
        const size_t nl = text.find('\n');
        std::string_view line = nl == std::string_view::npos ? text : text.substr(0, nl);
        text = nl == std::string_view::npos ? std::string_view{} : text.substr(nl + 1);
        fn(++line_no, line);
    }
}

bool is_comment_or_blank(std::string_view t) { return t.empty() || t.front() == '#' || t.front() == ';'; }

}  // namespace

ParseResult parse_settings(std::string_view text) {
    ParseResult result;
    for_each_line(text, [&](int line_no, std::string_view raw) {
        const std::string_view line = trim(raw);
        if (is_comment_or_blank(line)) return;
        const size_t eq = line.find('=');
        if (eq == std::string_view::npos) {
            result.warnings.push_back({line_no, std::string(), SettingsWarning::Reason::MalformedLine});
            return;
        }
        const std::string_view key = trim(line.substr(0, eq));
        const std::string_view value = trim(line.substr(eq + 1));
        const int k = find_key(key);
        if (k < 0) {
            result.warnings.push_back({line_no, std::string(key), SettingsWarning::Reason::UnknownKey});
            return;
        }
        bool ok = false;
        Settings& s = result.settings;
        bool b = false;
        int n = 0;
        switch (kKeys[static_cast<size_t>(k)].kind) {
            case Kind::Bool:
                ok = parse_bool(value, b);
                if (ok) {
                    switch (k) {
                        case 0: s.minimize_on_close = b; break;
                        case 1: s.auto_start_service = b; break;
                        case 2: s.low_battery_alert = b; break;
                        case 3: s.nintendo_mode = b; break;
                        case 4: s.hair_trigger = b; break;
                        default: s.exclusive_grab = b; break;
                    }
                }
                break;
            case Kind::Index:
                ok = parse_index(value, kKeys[static_cast<size_t>(k)].max_index, n);
                if (ok) {
                    if (k == 6) s.deadzone = n;
                    else if (k == 7) s.polling_rate = n;
                    else s.response_curve = n;
                }
                break;
            case Kind::Language:
                ok = parse_language(value, s.language);
                break;
        }
        if (!ok) result.warnings.push_back({line_no, std::string(key), SettingsWarning::Reason::InvalidValue});
    });
    return result;
}

std::string serialize_settings(const Settings& s, std::string_view existing) {
    std::string out;
    std::array<bool, kKeys.size()> written{};

    if (existing.empty()) {
        out += "# Ultimate2CFixer settings. Lines starting with # are comments.\n";
        out += "# Boolean values: 1 = on, 0 = off. ExclusiveGrab (Linux): hide the real controller from other programs while connected. Deadzone: 0 off, 1 low (8%), 2 normal (12%), 3 high (20%).\n";
        out += "# PollingRate: 0 = 125 Hz, 1 = 250 Hz, 2 = 500 Hz, 3 = 1000 Hz. ResponseCurve: 0 linear, 1 smooth aim, 2 aggressive.\n";
        out += "# Language: en, tr or es; leave empty to use the system language.\n";
    }

    for_each_line(existing, [&](int, std::string_view raw) {
        const std::string_view line = trim(raw);
        if (!is_comment_or_blank(line)) {
            const size_t eq = line.find('=');
            if (eq != std::string_view::npos) {
                const int k = find_key(trim(line.substr(0, eq)));
                if (k >= 0) {
                    if (written[static_cast<size_t>(k)]) return;   // duplicate: drop it, the first one is updated
                    written[static_cast<size_t>(k)] = true;
                    out += kKeys[static_cast<size_t>(k)].name;
                    out += '=';
                    out += value_text(s, k);
                    out += '\n';
                    return;
                }
            }
        }
        // kept as it was, without a trailing "\r"
        std::string_view keep = raw;
        while (!keep.empty() && keep.back() == '\r') keep.remove_suffix(1);
        out.append(keep);
        out += '\n';
    });

    for (size_t k = 0; k < kKeys.size(); ++k) {
        if (written[k]) continue;
        out += kKeys[k].name;
        out += '=';
        out += value_text(s, static_cast<int>(k));
        out += '\n';
    }
    return out;
}

MapConfig to_map_config(const Settings& s) {
    MapConfig c;
    c.deadzone = kDeadzonePresets[s.deadzone];
    c.curve = s.response_curve;
    c.hair_trigger = s.hair_trigger;
    c.nintendo_mode = s.nintendo_mode;
    return c;
}

int polling_hz(const Settings& s) { return kPollingRates[s.polling_rate]; }

}  // namespace u2c
