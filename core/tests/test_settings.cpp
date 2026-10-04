#include <string>

#include "check.h"
#include "u2c/settings.h"

using namespace u2c;

namespace {

bool has_warning(const ParseResult& r, SettingsWarning::Reason reason, int line) {
    for (const auto& w : r.warnings)
        if (w.reason == reason && w.line == line) return true;
    return false;
}

void test_defaults() {
    const Settings def;  // the Windows defaults
    CHECK(def.minimize_on_close && def.auto_start_service && def.low_battery_alert);
    CHECK(!def.nintendo_mode && !def.hair_trigger);
    CHECK(def.exclusive_grab);  // on by default on Linux
    CHECK_EQ(def.deadzone, 2);
    CHECK_EQ(def.polling_rate, 1);
    CHECK_EQ(def.response_curve, 0);
    CHECK(def.language.empty());

    ParseResult r = parse_settings("");
    CHECK(r.settings == def);
    CHECK(r.warnings.empty());
    r = parse_settings("# only a comment\n\n   \n; another\n");
    CHECK(r.settings == def);
    CHECK(r.warnings.empty());
    CHECK_EQ(to_map_config(def).deadzone, 4000);  // preset 2 = Normal (12%)
    CHECK_EQ(polling_hz(def), 250);
}

void test_valid_values() {
    ParseResult r = parse_settings(
        "MinimizeOnClose=0\nAutoStart=false\nLowBatteryAlert=0\nNintendoMode=1\nHairTrigger=TRUE\nExclusiveGrab=0\n"
        "Deadzone=3\nPollingRate=0\nResponseCurve=2\nLanguage=TR\n");
    CHECK(r.warnings.empty());
    CHECK(!r.settings.minimize_on_close);
    CHECK(!r.settings.auto_start_service);
    CHECK(!r.settings.low_battery_alert);
    CHECK(r.settings.nintendo_mode);
    CHECK(r.settings.hair_trigger);
    CHECK(!r.settings.exclusive_grab);
    CHECK_EQ(r.settings.deadzone, 3);
    CHECK_EQ(r.settings.polling_rate, 0);
    CHECK_EQ(r.settings.response_curve, 2);
    CHECK(r.settings.language == "tr");  // normalized to lower case
    CHECK_EQ(to_map_config(r.settings).deadzone, 6500);
    CHECK_EQ(to_map_config(r.settings).curve, 2);
    CHECK_EQ(polling_hz(r.settings), 125);

    // spaces, tabs, CRLF, BOM, case-insensitive keys, last duplicate wins
    r = parse_settings("\xEF\xBB\xBF  deadzone \t=\t 1 \r\nDEADZONE=3\r\n  Language = es\r\n");
    CHECK(r.warnings.empty());
    CHECK_EQ(r.settings.deadzone, 3);
    CHECK(r.settings.language == "es");
}

void test_bad_input() {
    const Settings def;
    ParseResult r = parse_settings("Deadzone=7\nPollingRate=abc\nResponseCurve=\nHairTrigger=2\nMinimizeOnClose=yes\nLanguage=fr\n");
    CHECK(r.settings == def);
    CHECK_EQ(r.warnings.size(), 6);
    for (int line = 1; line <= 6; ++line) CHECK(has_warning(r, SettingsWarning::Reason::InvalidValue, line));

    r = parse_settings("Deadzone=-1\nDeadzone=1x\nDeadzone=0003\nDeadzone=99999999999999999999\n");
    CHECK_EQ(r.settings.deadzone, 2);  // all four are invalid (leading zeros are not accepted either)
    for (int line = 1; line <= 4; ++line) CHECK(has_warning(r, SettingsWarning::Reason::InvalidValue, line));

    r = parse_settings("Deadzone=3 # comment\n");  // inline comments are not supported, so this value is invalid
    CHECK_EQ(r.settings.deadzone, 2);
    CHECK(has_warning(r, SettingsWarning::Reason::InvalidValue, 1));

    r = parse_settings("no equals sign\nSomethingElse=1\n=5\nDeadzone=1\n");
    CHECK(has_warning(r, SettingsWarning::Reason::MalformedLine, 1));
    CHECK(has_warning(r, SettingsWarning::Reason::UnknownKey, 2));
    CHECK(has_warning(r, SettingsWarning::Reason::UnknownKey, 3));
    CHECK_EQ(r.settings.deadzone, 1);  // later good lines still apply

    // an invalid value after a valid one keeps the earlier valid value
    r = parse_settings("Deadzone=3\nDeadzone=oops\n");
    CHECK_EQ(r.settings.deadzone, 3);

    // an empty language means no saved choice, not an error
    r = parse_settings("Language=\n");
    CHECK(r.settings.language.empty());
    CHECK(r.warnings.empty());

    // a big file of garbage must not crash or take long
    std::string big;
    for (int i = 0; i < 20000; ++i) big += "garbage line without equals " + std::to_string(i) + "\n";
    big += "Deadzone=1\n";
    r = parse_settings(big);
    CHECK_EQ(r.settings.deadzone, 1);
    CHECK_EQ(r.warnings.size(), 20000);
    std::string binary(5000, '\0');
    binary += "\xFF\xFE=\n=\n";
    r = parse_settings(binary);  // no crash is the check
    CHECK(true);
}

void test_roundtrip_all_combinations() {
    long combos = 0;
    for (int bits = 0; bits < 64; ++bits)
        for (int dz = 0; dz < 4; ++dz)
            for (int pr = 0; pr < 4; ++pr)
                for (int rc = 0; rc < 3; ++rc)
                    for (const char* lang : {"", "en", "tr", "es"}) {
                        Settings s;
                        s.minimize_on_close = bits & 1;
                        s.auto_start_service = bits & 2;
                        s.low_battery_alert = bits & 4;
                        s.nintendo_mode = bits & 8;
                        s.hair_trigger = bits & 16;
                        s.exclusive_grab = bits & 32;
                        s.deadzone = dz; s.polling_rate = pr; s.response_curve = rc;
                        s.language = lang;
                        const std::string text = serialize_settings(s, "");
                        const ParseResult r = parse_settings(text);
                        ++combos;
                        CHECK(r.settings == s);
                        CHECK(r.warnings.empty());
                    }
    CHECK_EQ(combos, 64 * 4 * 4 * 3 * 4);
}

void test_serialize_keeps_user_content() {
    const std::string existing =
        "# my own comment\r\n"
        "Deadzone=1\r\n"
        "\r\n"
        "CustomNote=keep me\r\n"
        "deadzone=2\r\n"  // duplicate: dropped, the first one is updated
        "nonsense line\r\n"
        "HairTrigger=0\r\n";
    Settings s;
    s.deadzone = 3;
    s.hair_trigger = true;
    s.language = "es";
    const std::string out = serialize_settings(s, existing);

    CHECK(out.find("# my own comment\n") == 0);  // first line untouched, line endings normalized
    CHECK(out.find("\r") == std::string::npos);
    CHECK(out.find("CustomNote=keep me\n") != std::string::npos);
    CHECK(out.find("nonsense line\n") != std::string::npos);
    CHECK(out.find("Deadzone=3\n") != std::string::npos);
    CHECK(out.find("deadzone=2") == std::string::npos);
    CHECK(out.find("HairTrigger=1\n") != std::string::npos);
    CHECK(out.find("Language=es\n") != std::string::npos);
    CHECK(out.find("PollingRate=1\n") != std::string::npos);
    CHECK(out.find("Deadzone=3\n", out.find("Deadzone=3\n") + 1) == std::string::npos);

    const ParseResult r = parse_settings(out);
    CHECK(r.settings == s);
    CHECK_EQ(r.warnings.size(), 2);  // CustomNote (unknown) and "nonsense line" (malformed) are kept on purpose

    // writing the result again changes nothing
    CHECK(serialize_settings(s, out) == out);

    const std::string fresh = serialize_settings(Settings{}, "");
    CHECK(fresh.rfind("# Ultimate2CFixer settings.", 0) == 0);
    CHECK(parse_settings(fresh).warnings.empty());
}

}  // namespace

void test_settings() {
    test_defaults();
    test_valid_values();
    test_bad_input();
    test_roundtrip_all_combinations();
    test_serialize_keeps_user_content();
}
