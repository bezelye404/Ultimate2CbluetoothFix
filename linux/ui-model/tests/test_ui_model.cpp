#include <cstdlib>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>

#include "../../../core/tests/check.h"
#include "u2c/ui/demo.h"
#include "u2c/ui/view_model.h"

using namespace u2c;
using namespace u2c::ui;

namespace {

std::string read_file(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    std::stringstream ss;
    ss << in.rdbuf();
    return ss.str();
}
// CTest sets U2C_RESOURCE_DIR. Run from the repository folder without it, the default below is used.
std::string resource_dir() {
    const char* dir = std::getenv("U2C_RESOURCE_DIR");
    return dir && dir[0] ? dir : "core/resources";
}
Translations load(const char* lang) {
    return Translations::parse(read_file(resource_dir() + "/lang/" + lang + ".txt"));
}
Catalog catalog(const char* lang) { return Catalog(load("en"), load(lang)); }

double channel(uint8_t v) {
    const double c = v / 255.0;
    return c <= 0.03928 ? c / 12.92 : std::pow((c + 0.055) / 1.055, 2.4);
}
double luminance(Rgb c) { return 0.2126 * channel(c.r) + 0.7152 * channel(c.g) + 0.0722 * channel(c.b); }
double contrast(ColorId a, ColorId b) {
    double la = luminance(color(a)), lb = luminance(color(b));
    if (la < lb) std::swap(la, lb);
    return (la + 0.05) / (lb + 0.05);
}
double saturation(Rgb c) {
    const double r = c.r / 255.0, g = c.g / 255.0, b = c.b / 255.0;
    const double mx = std::max({r, g, b}), mn = std::min({r, g, b});
    return mx == 0 ? 0 : (mx - mn) / mx;
}

void test_palette() {
    using C = ColorId;
    // Text colors must reach 4.5:1 (WCAG AA) against every background they are drawn on.
    struct Pair { C text, bg; const char* what; };
    const Pair pairs[] = {
        {C::TextPrimary, C::WindowBg, "title on window"}, {C::TextPrimary, C::CardBg, "text on card"},
        {C::TextPrimary, C::ButtonBg, "button text"}, {C::TextPrimary, C::ButtonHover, "button text pressed"},
        {C::AccentText, C::AccentBg, "accent button"}, {C::AccentText, C::AccentHover, "accent button pressed"},
        {C::TextSecondary, C::CardBg, "secondary on card"}, {C::TextSecondary, C::WindowBg, "secondary on window"},
        {C::TextMuted, C::CardBg, "muted on card"}, {C::TextMuted, C::WindowBg, "muted on window"},
        {C::TextMuted, C::TileBg, "idle tile label"}, {C::TextMuted, C::StickBg, "muted on stick bg"},
        {C::TextMuted, C::BadgeBg, "muted on badge"}, {C::TextMuted, C::DisabledBg, "disabled button text"},
        {C::Green, C::BadgeBg, "XBOX badge"}, {C::Amber, C::BadgeBg, "NINTENDO badge"},
        {C::Green, C::CardBg, "green on card"}, {C::Amber, C::CardBg, "amber on card"}, {C::Red, C::CardBg, "red text on card"},
        {C::OnGreen, C::Green, "label on a lit tile"},
    };
    for (const Pair& p : pairs) {
        const double cr = contrast(p.text, p.bg);
        if (cr < 4.5) std::printf("  low contrast %.2f: %s\n", cr, p.what);
        CHECK(cr >= 4.5);
    }
    // Marks that are not text (dots, bars, lit tiles) need 3:1.
    for (C mark : {C::Green, C::Amber, C::Red, C::Gray}) {
        CHECK(contrast(mark, C::CardBg) >= 3.0);
        CHECK(contrast(mark, C::TileBg) >= 3.0);
    }
    // Status colors are matte: HSV saturation at most 55%.
    for (C c : {C::Green, C::Amber, C::Red, C::Gray, C::TextMuted, C::TextSecondary, C::TextPrimary}) {
        if (saturation(color(c)) > 0.555) std::printf("  saturated color %d: %.2f\n", static_cast<int>(c), saturation(color(c)));
        CHECK(saturation(color(c)) <= 0.555);
    }
    // the chosen values, and that the saturated ones were really replaced
    CHECK(color(C::Green) == Rgb{0x5D, 0xBB, 0x8F});
    CHECK(color(C::TextMuted) == Rgb{0x8D, 0x8D, 0x97});
    CHECK(!(color(C::Green) == Rgb{0x10, 0xB9, 0x81}));
    // every color is defined (no accidental all-black entry)
    for (int i = 0; i < static_cast<int>(C::Count); ++i) CHECK(color(static_cast<C>(i)).r + color(static_cast<C>(i)).g + color(static_cast<C>(i)).b > 0);
}

bool inside(const Rect& r, int w, int h) { return r.x >= 0 && r.y >= 0 && r.right() <= w && r.bottom() <= h && r.w > 0 && r.h > 0; }
bool overlap(const Rect& a, const Rect& b) { return a.x < b.right() && b.x < a.right() && a.y < b.bottom() && b.y < a.bottom(); }
Rect shifted(const Rect& r, const Rect& origin) { return {origin.x + r.x, origin.y + r.y, r.w, r.h}; }

void test_layout() {
    const Rect* all[] = {&kLangButton, &kSettingsButton, &kTrayButton, &kCardStatus, &kCardTelemetry, &kCardBattery, &kStartButton,
                         &kStopButton, &kCardTerminal, &kClearButton, &kLogBox, &kSettingsCard, &kDeadzoneButton, &kPollingButton,
                         &kCurveButton, &kBackButton};
    for (const Rect* r : all) CHECK(inside(*r, kWindowW, kWindowH));
    // the three top cards do not touch each other, the buttons sit between cards and terminal
    CHECK(!overlap(kCardStatus, kCardTelemetry) && !overlap(kCardTelemetry, kCardBattery) && !overlap(kCardStatus, kCardBattery));
    CHECK(!overlap(kStartButton, kStopButton));
    CHECK(kStartButton.y >= kCardStatus.bottom() && kStartButton.bottom() <= kCardTerminal.y);
    CHECK_EQ(kCardBattery.right(), kWindowW - 24);
    CHECK_EQ(kCardTerminal.bottom(), kWindowH - 20);
    CHECK(kLogBox.x >= kCardTerminal.x && kLogBox.right() <= kCardTerminal.right() && kLogBox.bottom() <= kCardTerminal.bottom());
    CHECK(kLogBox.y > kClearButton.bottom());
    CHECK(!overlap(kLangButton, kSettingsButton) && !overlap(kSettingsButton, kTrayButton));
    // the telemetry contents fit in their card
    const Rect card = kCardTelemetry;
    const int vx = kVisOffsetX, vy = kVisOffsetY;
    const Rect parts[] = {
        {vx, vy, kStickBoxSize, kStickBoxSize}, {vx + kStickBoxGap, vy, kStickBoxSize, kStickBoxSize},
        {kBumper[0].x + vx, kBumper[0].y + vy, kBumper[0].w, kBumper[0].h}, {kBumper[1].x + vx, kBumper[1].y + vy, kBumper[1].w, kBumper[1].h},
        {kTriggerX[0] + vx, kTriggerY + vy, kTriggerW, kTriggerH}, {kTriggerX[1] + vx, kTriggerY + vy, kTriggerW, kTriggerH},
        kBadge,
    };
    for (const Rect& p : parts) CHECK(inside(p, card.w, card.h));
    for (int i = 0; i < 4; ++i) CHECK(inside({vx + kAbxyOriginX + kTileDx[i], vy + kTileDy[i], kTileSize, kTileSize}, card.w, card.h));
    CHECK(!overlap(parts[0], parts[1]));
    CHECK(!overlap(parts[2], parts[3]));
    // Windows values, to catch accidental edits
    CHECK_EQ(kWindowW, 840); CHECK_EQ(kWindowH, 580);
    CHECK(kCardStatus.x == 24 && kCardStatus.y == 56 && kCardStatus.w == 240 && kCardStatus.h == 96);
    CHECK(kCardTelemetry.x == 278 && kCardTelemetry.w == 280);
    CHECK(kCardBattery.x == 572 && kCardBattery.w == 244);
    CHECK(kBumper[0].x == 134 && kBumper[1].x == 168 && kTriggerX[0] == 210 && kTriggerX[1] == 223);
    CHECK(kTileDx[0] == 2 && kTileDy[0] == 11 && kTileDx[1] == 15 && kTileDy[1] == 0 && kTileDx[3] == 28);
    // the settings screen: seven checkboxes, buttons below the last one and inside the card
    CHECK_EQ(kCheckboxCount, 7);
    CHECK_EQ(static_cast<int>(sizeof(kCheckboxes) / sizeof(kCheckboxes[0])), kCheckboxCount);
    CHECK_EQ(checkbox_y(0), 106);
    CHECK_EQ(checkbox_y(5), 256);  // the six Windows rows keep their places
    CHECK(checkbox_y(6) + kCheckboxH <= kDeadzoneButton.y);
    CHECK(kBackButton.bottom() <= kSettingsCard.bottom());
    CHECK(!overlap(kDeadzoneButton, kPollingButton) && !overlap(kCurveButton, kBackButton));
    CHECK(inside({kCheckboxX, checkbox_y(6), kCheckboxW, kCheckboxH}, kSettingsCard.right(), kSettingsCard.bottom()));
    (void)shifted;
}

void test_display() {
    const Catalog en = catalog("en"), tr = catalog("tr");
    auto s = status_view(en, ServiceStatus::Connected, "8BitDo Ultimate 2C Wireless");
    CHECK(s.dot == ColorId::Green && s.status_text == "Connected & Active" && s.name_text == "8BitDo Ultimate 2C Wireless");
    s = status_view(en, ServiceStatus::Connected, "");
    CHECK(s.name_text == "No Device Detected");
    s = status_view(en, ServiceStatus::Searching, "ignored name");
    CHECK(s.dot == ColorId::Amber && s.status_text == "Searching for controller..." && s.name_text == "No Device Detected");
    s = status_view(en, ServiceStatus::Disconnected, "");
    CHECK(s.dot == ColorId::Red && s.status_text == "Waiting for controller...");
    s = status_view(en, ServiceStatus::Stopped, "");
    CHECK(s.dot == ColorId::Gray && s.status_text == "Service Stopped");
    CHECK(status_view(tr, ServiceStatus::Stopped, "").status_text == "Servis Durduruldu");

    for (int level = -1; level <= 100; ++level) {
        const BatteryView b = battery_view(en, level);
        if (level < 0) {
            CHECK(b.percent_text == "--%" && b.fill_percent == 0 && b.status_text == "No Device Detected" && b.status_color == ColorId::TextMuted);
            continue;
        }
        CHECK(b.percent_text == std::to_string(level) + "%");
        CHECK_EQ(b.fill_percent, level);  // 0 stays 0, 1..100 stay as they are
        CHECK(b.bar_color == (level <= 20 ? ColorId::Red : level <= 50 ? ColorId::Amber : ColorId::Green));
        CHECK(b.status_text == (level <= 20 ? "Low Battery - Please Recharge" : level <= 50 ? "Wireless - Moderate Level" : "Wireless - Healthy Level"));
        CHECK(b.status_color == (level <= 20 ? ColorId::Red : ColorId::TextSecondary));
    }
    CHECK_EQ(battery_view(en, 250).fill_percent, 100);  // never more than 100
    CHECK_EQ(battery_fill_width(212, 50), 106);
    CHECK_EQ(battery_fill_width(212, 0), 0);
    CHECK_EQ(battery_fill_width(212, 100), 212);
    CHECK(battery_view(tr, 10).status_text == "Düşük Pil - Lütfen Şarj Edin");

    CHECK(std::string(mode_badge(false).text) == "XBOX" && mode_badge(false).color == ColorId::Green);
    CHECK(std::string(mode_badge(true).text) == "NINTENDO" && mode_badge(true).color == ColorId::Amber);
    auto t = abxy_tiles(false, kA);
    CHECK(t[0].label == 'X' && t[1].label == 'Y' && t[2].label == 'A' && t[3].label == 'B');
    CHECK(!t[0].lit && !t[1].lit && t[2].lit && !t[3].lit);
    t = abxy_tiles(true, kA);  // Nintendo: Y left, X top, B bottom, A right; the report bit A lights "B"
    CHECK(t[0].label == 'Y' && t[1].label == 'X' && t[2].label == 'B' && t[3].label == 'A');
    CHECK(!t[0].lit && !t[1].lit && t[2].lit && !t[3].lit);
    t = abxy_tiles(true, kA | kB | kX | kY);
    CHECK(t[0].lit && t[1].lit && t[2].lit && t[3].lit);
    t = abxy_tiles(false, kLeftShoulder | kDpadUp | kStart);
    CHECK(!t[0].lit && !t[1].lit && !t[2].lit && !t[3].lit);
    CHECK(bumper_lit(kLeftShoulder, true) && !bumper_lit(kLeftShoulder, false));
    CHECK(bumper_lit(kRightShoulder, false) && !bumper_lit(kRightShoulder, true));

    // stick dot and trigger bars: integer math
    CHECK(stick_dot_offset(0, 0).dx == 0 && stick_dot_offset(0, 0).dy == 0);
    CHECK(stick_dot_offset(32767, 32767).dx == 11 && stick_dot_offset(32767, 32767).dy == -11);  // up on the stick is up on the screen
    CHECK(stick_dot_offset(-32768, -32768).dx == -12 && stick_dot_offset(-32768, -32768).dy == 12);
    CHECK(stick_dot_offset(16384, -16384).dx == 6 && stick_dot_offset(16384, -16384).dy == 6);
    for (int v = -32768; v <= 32767; v += 97) {
        const Offset o = stick_dot_offset(static_cast<int16_t>(v), static_cast<int16_t>(v));
        CHECK(std::abs(o.dx) <= kStickTravel && std::abs(o.dy) <= kStickTravel);  // the dot never leaves the box
    }
    CHECK_EQ(trigger_fill_height(0), 0);
    CHECK_EQ(trigger_fill_height(255), kTriggerH);
    CHECK_EQ(trigger_fill_height(128), 15);
    for (int v = 0; v < 256; ++v) CHECK(trigger_fill_height(static_cast<uint8_t>(v)) <= kTriggerH);

    // tray tooltip
    CHECK(tray_tooltip(en, ServiceStatus::Connected, 87) == "Ultimate2CFixer: Connected & Active (87%)");
    CHECK(tray_tooltip(en, ServiceStatus::Connected, -1) == "Ultimate2CFixer: Connected & Active");
    CHECK(tray_tooltip(en, ServiceStatus::Searching, 50) == "Ultimate2CFixer: Searching for controller...");
    CHECK(tray_tooltip(en, ServiceStatus::Disconnected, 50) == "Ultimate2CFixer: Waiting for controller...");  // fixed on Linux (the Windows version shows "Service Stopped" here)
    CHECK(tray_tooltip(en, ServiceStatus::Stopped, 50) == "Ultimate2CFixer: Service Stopped");
    CHECK(tray_tooltip(tr, ServiceStatus::Connected, 87) == "Ultimate2CFixer: Bağlı ve Aktif (87%)");

    CHECK(deadzone_text(en, 2) == "Stick Drift / Deadzone: Normal (12%)");
    CHECK(deadzone_text(tr, 0) == "Stick Drift / Ölü Bölge: Kapalı (%0)");
    CHECK(polling_text(en, 3) == "Polling Rate: 1000 Hz");
    CHECK(polling_text(en, 0) == "Polling Rate: 125 Hz");
    CHECK(curve_text(en, 1) == "Stick Curve: Smooth Aim");
    CHECK(deadzone_text(en, 99) == deadzone_text(en, 3));  // out of range is clamped, never crashes
    CHECK(curve_text(en, -5) == curve_text(en, 0));
}

void test_rate_readout() {
    const Catalog en = catalog("en"), tr = catalog("tr");
    constexpr int64_t s = 1000000000LL;
    RateReadout r;
    CHECK(r.view(0).idle);  // nothing measured yet
    r.update(0, 0.0f, 1 * s);
    CHECK(r.view(1 * s).idle);  // zero is not a value to show
    r.update(250, 4.0f, 2 * s);
    CHECK(!r.view(2 * s).idle && r.view(2 * s).hz == 250);
    r.update(0, 0.0f, 3 * s);  // input stopped: the last value is kept for 2 seconds
    CHECK(!r.view(3 * s).idle && r.view(3 * s).hz == 250);
    CHECK(!r.view(2 * s + RateReadout::kHoldNs - 1).idle);
    CHECK(r.view(2 * s + RateReadout::kHoldNs).idle);  // then "Idle"
    r.update(120, 8.3f, 10 * s);
    CHECK(r.view(10 * s).hz == 120);
    r.reset();
    CHECK(r.view(10 * s).idle);
    CHECK(rate_text(en, RateReadout::View{false, 250, 4.0f}) == "250 Hz \xE2\x80\xA2 4.0 ms");
    CHECK(rate_text(en, RateReadout::View{true, 0, 0}) == "Idle");
    CHECK(rate_text(tr, RateReadout::View{true, 0, 0}) == "Boşta");
    CHECK(format_ms(4.0f) == "4.0");
    CHECK(format_ms(3.96f) == "4.0");
    CHECK(format_ms(0.0f) == "0.0");
    CHECK(format_ms(1000.0f) == "1000.0");
    CHECK(format_ms(8.33f) == "8.3");
    CHECK(rate_text(tr, RateReadout::View{false, 1000, 1.0f}) == "1000 Hz \xE2\x80\xA2 1.0 ms");  // always a dot, also in Turkish
}

void test_low_battery_alert() {
    LowBatteryAlert a;
    CHECK(!a.update(-1, true));
    CHECK(!a.update(0, true));  // 0 means unknown or empty: no alert
    CHECK(!a.update(16, true));
    CHECK(a.update(15, true));  // fires once
    CHECK(!a.update(14, true));
    CHECK(!a.update(10, true));
    CHECK(!a.update(18, true));  // 16 to 20: not armed again yet
    CHECK(!a.update(12, true));
    CHECK(!a.update(21, true));  // above 20: armed again
    CHECK(a.update(9, true));
    LowBatteryAlert off;
    CHECK(!off.update(5, false));  // the setting is off
    CHECK(off.update(5, true));  // and it fires when it is switched on later
}

void test_text_state() {
    CHECK(format_log_line({1, 2, 3}, "hi") == "[01:02:03] hi");
    CHECK(format_log_line({23, 59, 59}, "") == "[23:59:59] ");
    LogBuffer log;
    CHECK(log.text().empty());
    for (int i = 0; i < 150; ++i) log.push({0, 0, i % 60}, "line " + std::to_string(i));
    CHECK_EQ(log.size(), 100);
    const std::string text = log.text();
    CHECK(text.find("line 49\n") == std::string::npos);  // dropped
    CHECK(text.rfind("] line 50\n", text.find("line 50") + 20) != std::string::npos || text.find("line 50\n") != std::string::npos);
    CHECK(text.substr(text.size() - 8) == "line 149");
    CHECK(text.back() != '\n');
    log.clear();
    CHECK_EQ(log.size(), 0);

    const std::vector<std::string> all = {"en", "tr", "es"}, two = {"en", "tr"};
    CHECK(detect_language("tr_TR.UTF-8", all) == "tr");
    CHECK(detect_language("TR_tr", all) == "tr");
    CHECK(detect_language("es_419", all) == "es");
    CHECK(detect_language("es_ES.UTF-8", all) == "es");
    CHECK(detect_language("es_ES.UTF-8", two) == "en");  // "es" is not in the list
    CHECK(detect_language("en_US.UTF-8", all) == "en");
    CHECK(detect_language("de_DE", all) == "en");
    CHECK(detect_language("C", all) == "en");
    CHECK(detect_language("POSIX", all) == "en");
    CHECK(detect_language("", all) == "en");
    CHECK(detect_language("tr", all) == "tr");
    CHECK(detect_language("tr@euro", all) == "tr");
    CHECK(next_language("en", all) == "tr" && next_language("tr", all) == "es" && next_language("es", all) == "en");
    CHECK(next_language("en", two) == "tr" && next_language("tr", two) == "en");
    CHECK(next_language("xx", all) == "en");  // unknown current: start from the first
    CHECK(next_language("en", {"en"}) == "en");
    CHECK(next_language("en", {}) == "en");
    CHECK(language_button_text("en", all) == "TR");  // shows the language it switches to
    CHECK(language_button_text("tr", all) == "ES");
    CHECK(language_button_text("es", all) == "EN");
    CHECK(language_button_text("tr", two) == "EN");
    CHECK(effective_language("tr", "en_US", all) == "tr");  // a saved choice wins
    CHECK(effective_language("", "tr_TR", all) == "tr");  // otherwise the system language
    CHECK(effective_language("es", "tr_TR", two) == "tr");  // a saved language without a file falls back to detection
    CHECK(effective_language("", "", all) == "en");

    Settings s;
    for (const CheckboxSpec& c : kCheckboxes) {
        if (c.id == SettingId::StartWithSystem) { CHECK(!get_setting(s, c.id)); set_setting(s, c.id, true); CHECK(!get_setting(s, c.id)); continue; }
        const bool before = get_setting(s, c.id);
        set_setting(s, c.id, !before);
        CHECK(get_setting(s, c.id) == !before);
        set_setting(s, c.id, before);
        CHECK(get_setting(s, c.id) == before);
    }
    CHECK(s == Settings{});  // setting and resetting changed nothing else
    set_setting(s, SettingId::ExclusiveGrab, false);
    CHECK(!s.exclusive_grab);
    for (const CheckboxSpec& a : kCheckboxes) {
        if (a.id == SettingId::StartWithSystem) continue;
        Settings t;
        set_setting(t, a.id, !get_setting(t, a.id));
        int changed = 0;
        for (const CheckboxSpec& b : kCheckboxes)
            if (b.id != SettingId::StartWithSystem && get_setting(t, b.id) != get_setting(Settings{}, b.id)) ++changed;
        CHECK_EQ(changed, 1);
    }
    CHECK_EQ(next_deadzone(0), 1); CHECK_EQ(next_deadzone(3), 0);
    CHECK_EQ(next_polling_rate(3), 0); CHECK_EQ(next_polling_rate(1), 2);
    CHECK_EQ(next_curve(2), 0); CHECK_EQ(next_curve(0), 1);
}

void test_policy() {
    CHECK(on_close_requested(true, true) == CloseAction::HideToTray);
    CHECK(on_close_requested(false, true) == CloseAction::Quit);
    CHECK(on_close_requested(true, false) == CloseAction::Quit);  // no tray: a hidden window could never come back
    CHECK(on_close_requested(false, false) == CloseAction::Quit);
    CHECK(on_tray_button(true) == MinimizeAction::HideToTray);
    CHECK(on_tray_button(false) == MinimizeAction::MinimizeWindow);
    CHECK(startup_action(false, true) == StartupAction::ShowNormal);
    CHECK(startup_action(false, false) == StartupAction::ShowNormal);
    CHECK(startup_action(true, true) == StartupAction::HideToTray);
    CHECK(startup_action(true, false) == StartupAction::ShowMinimized);

    CHECK(ui_notice_from(NoticeKind::NoControllerAccess) == UiNotice::NoControllerAccess);
    CHECK(ui_notice_from(NoticeKind::NoVirtualDevice) == UiNotice::NoVirtualDevice);
    CHECK(ui_notice_from(NoticeKind::NoBluetoothService) == UiNotice::NoBluetoothService);

    CHECK(detect_distro("NAME=\"Arch Linux\"\nID=arch\nBUILD_ID=rolling\n") == Distro::Arch);
    CHECK(detect_distro("NAME=\"CachyOS Linux\"\nID=cachyos\nID_LIKE=\"arch\"\n") == Distro::Arch);
    CHECK(detect_distro("ID=manjaro\nID_LIKE=arch\n") == Distro::Arch);
    CHECK(detect_distro("ID=ubuntu\nID_LIKE=debian\n") == Distro::Debian);
    CHECK(detect_distro("ID=debian\n") == Distro::Debian);
    CHECK(detect_distro("ID=linuxmint\nID_LIKE=\"ubuntu debian\"\n") == Distro::Debian);
    CHECK(detect_distro("ID=pop\r\nID_LIKE=\"ubuntu debian\"\r\n") == Distro::Debian);
    CHECK(detect_distro("ID=fedora\n") == Distro::Other);
    CHECK(detect_distro("ID=opensuse-tumbleweed\nID_LIKE=\"opensuse suse\"\n") == Distro::Other);
    CHECK(detect_distro("") == Distro::Other);
    CHECK(detect_distro("garbage") == Distro::Other);
    CHECK(detect_distro("ID='arch'\n") == Distro::Arch);  // single quotes
    CHECK(detect_distro("ID=archlike\n") == Distro::Other);  // only the exact word counts

    const UiNotice all[] = {UiNotice::NoControllerAccess, UiNotice::NoVirtualDevice, UiNotice::NoBluetoothService, UiNotice::NoTrayHost, UiNotice::WaylandPluginMissing};
    const Translations en = load("en"), tr = load("tr");
    for (UiNotice n : all) {
        for (Distro d : {Distro::Arch, Distro::Debian, Distro::Other}) {
            const NoticeContent c = notice_content(n, d);
            CHECK(!c.body_key.empty());
            CHECK(en.has(c.body_key) && tr.has(c.body_key));
            if (d == Distro::Other) CHECK(c.commands.empty());  // an unknown distribution gets no guessed commands
            else CHECK(!c.commands.empty());
            for (const auto& cmd : c.commands) CHECK(!cmd.empty() && cmd.find('\n') == std::string::npos);
            CHECK(c.commands_unverified);
        }
    }
    CHECK(notice_content(UiNotice::NoBluetoothService, Distro::Arch).commands[0] == "sudo pacman -S bluez bluez-utils");
    CHECK(notice_content(UiNotice::NoBluetoothService, Distro::Debian).commands[0] == "sudo apt install bluez");
    CHECK(notice_content(UiNotice::WaylandPluginMissing, Distro::Arch).commands[0] == "sudo pacman -S qt6-wayland");
    // the uinput commands contain the steam-devices rule and nothing that runs unasked
    const auto uin = notice_content(UiNotice::NoVirtualDevice, Distro::Arch).commands;
    CHECK(uin[0].find("TAG+=\"uaccess\"") != std::string::npos);
    CHECK(uin[0].find("/etc/udev/rules.d/") != std::string::npos);
    for (const char* key : kNoticeUiKeys) CHECK(en.has(key) && tr.has(key));
    for (const char* key : kUiKeys) {
        CHECK(en.has(key));
        CHECK(tr.has(key));
        if (!en.has(key) || !tr.has(key)) std::printf("  missing UI key %s\n", key);
    }
}

void test_view_model() {
    ViewModel vm(catalog("en"), Settings{}, true);
    CHECK(vm.status().status_text == "Service Stopped");
    CHECK(!vm.rate_visible());
    vm.on_status(ServiceStatus::Searching, "");
    CHECK(vm.status().dot == ColorId::Amber);
    vm.on_status(ServiceStatus::Connected, "Pad");
    CHECK(vm.status().name_text == "Pad" && vm.rate_visible());
    PadReport r; r.buttons = kA; r.lx = 1000;
    vm.on_input(r);
    CHECK(vm.input() == r);
    CHECK(vm.tiles()[2].lit);
    vm.on_status(ServiceStatus::Disconnected, "");  // the live input is cleared when the controller is gone
    CHECK(vm.input() == PadReport{});
    CHECK(!vm.rate_visible());

    constexpr int64_t s = 1000000000LL;
    vm.on_status(ServiceStatus::Connected, "Pad");
    ServiceState st;
    st.status = ServiceStatus::Connected; st.live_hz = 250; st.live_ms = 4.0f;
    vm.poll(st, 10 * s);
    CHECK(vm.rate(10 * s) == "250 Hz \xE2\x80\xA2 4.0 ms");
    CHECK(vm.rate(12 * s + 1) == "Idle");
    st.status = ServiceStatus::Searching; st.live_hz = 999;
    vm.poll(st, 13 * s);  // ignored while not connected
    CHECK(vm.rate(13 * s) == "Idle");

    vm.on_battery({50, "Pad"});
    CHECK(!vm.take_battery_alert());
    CHECK_EQ(vm.battery_level(), 50);
    vm.on_battery({14, "Pad"});
    CHECK(vm.take_battery_alert());
    CHECK(!vm.take_battery_alert());
    vm.on_battery({13, "Pad"});
    CHECK(!vm.take_battery_alert());
    CHECK(vm.battery().bar_color == ColorId::Red);
    CHECK(vm.tooltip() == "Ultimate2CFixer: Connected & Active (13%)");

    Settings quiet;
    quiet.low_battery_alert = false;
    ViewModel q(catalog("en"), quiet, true);
    q.on_battery({5, "Pad"});
    CHECK(!q.take_battery_alert());

    // log lines are formatted when they arrive, with the current language
    vm.on_log({"LogControllerConnected", {{"name", "Pad"}}}, {10, 20, 30});
    CHECK(vm.log_text() == "[10:20:30] Pad connected.");
    vm.set_catalog(catalog("tr"));
    vm.on_log({"LogBatteryLevel", {{"name", "Pad"}, {"level", "80"}}}, {10, 20, 31});
    CHECK(vm.log_text() == "[10:20:30] Pad connected.\n[10:20:31] Pad Pil: %80");  // the old line keeps its language
    vm.on_log({"NoSuchKey", {}}, {10, 20, 32});
    CHECK(vm.log_text().find("[NoSuchKey]") != std::string::npos);  // a missing key is visible, not silent
    vm.clear_log();
    CHECK(vm.log_text().empty());
    vm.set_catalog(catalog("en"));

    // notices: each kind once per run, in order
    vm.on_notice({NoticeKind::NoVirtualDevice, "/dev/uinput"});
    vm.on_notice({NoticeKind::NoVirtualDevice, "/dev/uinput"});
    vm.add_notice(UiNotice::NoTrayHost);
    vm.on_notice({NoticeKind::NoControllerAccess, "/dev/input/event9"});
    auto n = vm.take_notice();
    CHECK(n && n->kind == UiNotice::NoVirtualDevice && n->detail == "/dev/uinput");
    n = vm.take_notice();
    CHECK(n && n->kind == UiNotice::NoTrayHost);
    n = vm.take_notice();
    CHECK(n && n->kind == UiNotice::NoControllerAccess && n->detail == "/dev/input/event9");
    CHECK(!vm.take_notice().has_value());
    vm.on_notice({NoticeKind::NoVirtualDevice, "/dev/uinput"});  // already shown: not queued again
    CHECK(!vm.take_notice().has_value());

    CHECK_EQ(vm.settings().deadzone, 2);
    CHECK_EQ(vm.cycle_deadzone().deadzone, 3);
    CHECK_EQ(vm.cycle_deadzone().deadzone, 0);
    CHECK(vm.deadzone_label() == "Stick Drift / Deadzone: Off (0%)");
    CHECK_EQ(vm.cycle_polling_rate().polling_rate, 2);
    CHECK(vm.polling_label() == "Polling Rate: 500 Hz");
    CHECK_EQ(vm.cycle_curve().response_curve, 1);
    CHECK(vm.curve_label() == "Stick Curve: Smooth Aim");
    CHECK(vm.set_checkbox(SettingId::NintendoMode, true).nintendo_mode);
    CHECK(std::string(vm.badge().text) == "NINTENDO");
    CHECK(vm.tiles()[0].label == 'Y');
    CHECK(!vm.set_checkbox(SettingId::ExclusiveGrab, false).exclusive_grab);
    CHECK(vm.active_language() == "en");  // default until the program sets the detected one
    vm.set_active_language("es");
    CHECK(vm.active_language() == "es" && vm.settings().language.empty());  // detected, not saved
    CHECK(vm.set_language("tr").language == "tr");
    CHECK(vm.active_language() == "tr");
    // a full cycle of every cycling button returns to the start
    ViewModel c(catalog("en"), Settings{}, true);
    for (int i = 0; i < 4; ++i) c.cycle_deadzone();
    for (int i = 0; i < 4; ++i) c.cycle_polling_rate();
    for (int i = 0; i < 3; ++i) c.cycle_curve();
    CHECK(c.settings() == Settings{});
    c.set_tray_available(false);
    CHECK(!c.tray_available());
}

void test_three_languages() {
    std::vector<std::string> available;
    for (const char* code : kAllLanguages)
        if (!read_file(resource_dir() + "/lang/" + code + ".txt").empty()) available.emplace_back(code);
    CHECK(available == std::vector<std::string>({"en", "tr", "es"}));
    CHECK(next_language("en", available) == "tr" && next_language("tr", available) == "es" && next_language("es", available) == "en");
    CHECK(detect_language("es_MX.UTF-8", available) == "es");
    CHECK(detect_language("es_AR", available) == "es");
    CHECK(effective_language("", "es_CO.UTF-8", available) == "es");
    CHECK(language_button_text("es", available) == "EN");
    // every key the window uses exists in Spanish too, and Spanish reads as Spanish in a few places
    const Translations es = load("es");
    for (const char* key : kUiKeys) CHECK(es.has(key));
    for (const char* key : kNoticeUiKeys) CHECK(es.has(key));
    const Catalog c = catalog("es");
    CHECK(status_view(c, ServiceStatus::Connected, "").status_text == "Conectado y activo");
    CHECK(battery_view(c, 10).status_text == "Batería baja - Carga el control");
    CHECK(tray_tooltip(c, ServiceStatus::Connected, 87) == "Ultimate2CFixer: Conectado y activo (87%)");
    CHECK(rate_text(c, RateReadout::View{true, 0, 0}) == "Inactivo");
    CHECK(deadzone_text(c, 2) == "Deriva / zona muerta: Normal (12%)");
    CHECK(polling_text(c, 3) == "Frecuencia de sondeo: 1000 Hz");
}

void test_translations_complete() {
    // the new keys exist in both languages and the Turkish notice texts are not copies of the English ones
    const Translations en = load("en"), tr = load("tr");
    for (const char* key : {"ExclusiveGrab", "ExclusiveGrabHelp", "Idle", "NoticeTitle", "NoticeNoTrayHost", "NoticeWaylandPlugin"})
        CHECK(en.get(key) != tr.get(key));
    CHECK(en.get("NoticeNoTrayHost") == "No system tray found. Closing the window quits the app. On GNOME, install the AppIndicator extension to get a tray icon.");
}

void test_demo() {
    uint32_t seen_keys = 0;
    bool seen_hat[4] = {false, false, false, false};
    int min_x = 255, max_x = 0;
    for (double t = 0; t < 120.0; t += 0.01) {
        const RawInput in = demo_input(t);
        for (int v : {in.abs_x, in.abs_y, in.abs_z, in.abs_rz, in.abs_brake, in.abs_gas}) CHECK(v >= 0 && v <= 255);
        CHECK(in.hat_x >= -1 && in.hat_x <= 1 && in.hat_y >= -1 && in.hat_y <= 1);
        CHECK(in.abs_brake <= 128 && in.abs_gas <= 128);  // the triggers stay in the lower half: visible movement
        seen_keys |= in.keys;
        if (in.hat_y < 0) seen_hat[0] = true;
        if (in.hat_x > 0) seen_hat[1] = true;
        if (in.hat_y > 0) seen_hat[2] = true;
        if (in.hat_x < 0) seen_hat[3] = true;
        min_x = std::min(min_x, in.abs_x);
        max_x = std::max(max_x, in.abs_x);
    }
    for (Key k : {kKeySouth, kKeyEast, kKeyNorth, kKeyWest, kKeyTl, kKeyTr}) CHECK((seen_keys & key_bit(k)) != 0);
    CHECK(seen_hat[0] && seen_hat[1] && seen_hat[2] && seen_hat[3]);
    CHECK(min_x < 10 && max_x > 245);  // the stick uses the whole range
    CHECK(demo_input(3.7).abs_x == demo_input(3.7).abs_x);
    CHECK(demo_input(-1.0).keys == demo_input(0.0).keys);  // negative time is harmless
}

}  // namespace

int main() {
    test_palette();
    test_layout();
    test_display();
    test_rate_readout();
    test_low_battery_alert();
    test_text_state();
    test_policy();
    test_view_model();
    test_demo();
    test_three_languages();
    test_translations_complete();
    std::printf("checks: %ld, failures: %ld\n", u2ctest::g_checks, u2ctest::g_failures);
    return u2ctest::g_failures == 0 ? 0 : 1;
}

void test_settings() {}
void test_translations() {}
void test_rate_limiter() {}
