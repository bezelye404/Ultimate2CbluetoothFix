#include <algorithm>
#include <vector>

#include "../../../core/tests/check.h"
#include "u2c/buttons.h"
#include "u2c/platform/evdev_logic.h"

using namespace u2c;
using namespace u2c::platform;

namespace {

InputEvent ev(uint16_t t, uint16_t c, int v) { return InputEvent{t, c, v}; }

std::vector<InputEvent> events_for(const PadReport* prev, const PadReport& next) {
    InputEvent buf[kMaxPadEvents];
    const size_t n = pad_events(prev, next, buf);
    return std::vector<InputEvent>(buf, buf + n);
}
bool contains(const std::vector<InputEvent>& v, uint16_t t, uint16_t c, int value) {
    return std::any_of(v.begin(), v.end(), [&](const InputEvent& e) { return e.type == t && e.code == c && e.value == value; });
}
int count_code(const std::vector<InputEvent>& v, uint16_t t, uint16_t c) {
    return static_cast<int>(std::count_if(v.begin(), v.end(), [&](const InputEvent& e) { return e.type == t && e.code == c; }));
}

struct KeySet { bool all = true; uint16_t missing = 0; };
bool has_key(void* ctx, uint16_t code) { auto* k = static_cast<KeySet*>(ctx); return k->all && code != k->missing; }

std::array<AbsInfo, 0x12> good_abs() {
    std::array<AbsInfo, 0x12> a{};
    for (uint16_t c : {kAbsX, kAbsY, kAbsZ, kAbsRz, kAbsGas, kAbsBrake}) a[c] = {true, 0, 255};
    for (uint16_t c : {kAbsHat0X, kAbsHat0Y}) a[c] = {true, -1, 1};
    return a;
}

void test_raw_state() {
    RawState s;
    CHECK(!s.apply(ev(kEvAbs, kAbsX, 255)));  // not a report yet
    CHECK(!s.apply(ev(kEvAbs, kAbsGas, 300)));  // clamped to 255
    CHECK(!s.apply(ev(kEvAbs, kAbsBrake, -5)));  // clamped to 0
    CHECK(!s.apply(ev(kEvAbs, kAbsHat0X, 7)));  // clamped to 1
    CHECK(!s.apply(ev(kEvKey, 0x130, 1)));
    CHECK(!s.apply(ev(kEvKey, 0x138, 1)));  // BTN_TL2 (the click)
    CHECK(!s.apply(ev(kEvKey, 0x999, 1)));  // unknown key: ignored
    CHECK(!s.apply(ev(kEvAbs, 0x20, 5)));  // unknown axis: ignored
    CHECK(s.apply(ev(kEvSyn, kSynReport, 0)));
    CHECK_EQ(s.raw().abs_x, 255);
    CHECK_EQ(s.raw().abs_gas, 255);
    CHECK_EQ(s.raw().abs_brake, 0);
    CHECK_EQ(s.raw().hat_x, 1);
    CHECK((s.raw().keys & key_bit(kKeySouth)) != 0);
    CHECK((s.raw().keys & key_bit(kKeyTl2)) != 0);
    s.apply(ev(kEvKey, 0x130, 2));  // auto repeat: nothing changes
    CHECK((s.raw().keys & key_bit(kKeySouth)) != 0);
    s.apply(ev(kEvKey, 0x130, 0));
    CHECK((s.raw().keys & key_bit(kKeySouth)) == 0);
    CHECK(!s.take_dropped());
    CHECK(!s.apply(ev(kEvSyn, kSynDropped, 0)));
    CHECK(s.take_dropped());
    CHECK(!s.take_dropped());
    s.reset();
    CHECK_EQ(s.raw().abs_x, 127);
    CHECK_EQ(s.raw().keys, 0);
}

void test_matcher() {
    KeySet keys;
    CHECK(matches_controller(0x2dc8, good_abs(), has_key, &keys));
    CHECK(!matches_controller(0x045e, good_abs(), has_key, &keys));  // another vendor (also our own virtual pad)
    CHECK(!matches_controller(0x0000, good_abs(), has_key, &keys));
    auto a = good_abs(); a[kAbsGas].present = false;
    CHECK(!matches_controller(0x2dc8, a, has_key, &keys));
    a = good_abs(); a[kAbsX].max = 65535;  // a different range: the scaling would be wrong
    CHECK(!matches_controller(0x2dc8, a, has_key, &keys));
    a = good_abs(); a[kAbsHat0Y].min = 0;
    CHECK(!matches_controller(0x2dc8, a, has_key, &keys));
    keys.missing = 0x136;
    CHECK(!matches_controller(0x2dc8, good_abs(), has_key, &keys));
    keys.missing = 0x13f;  // the unnamed 0x13f key is optional
    CHECK(matches_controller(0x2dc8, good_abs(), has_key, &keys));
    KeySet none; none.all = false;
    CHECK(!matches_controller(0x2dc8, good_abs(), has_key, &none));
}

void test_pad_events() {
    PadReport zero;
    auto all = events_for(nullptr, zero);
    CHECK_EQ(count_code(all, kEvAbs, kAbsX), 1);
    CHECK_EQ(count_code(all, kEvAbs, kAbsRy), 1);
    CHECK_EQ(count_code(all, kEvAbs, kAbsHat0Y), 1);
    int keys = 0;
    for (const auto& e : all) if (e.type == kEvKey) ++keys;
    CHECK_EQ(keys, 11);
    CHECK(all.back().type == kEvSyn && all.back().code == kSynReport);
    CHECK(contains(all, kEvAbs, kAbsY, -1));  // ~0 = -1: the rest value of an inverted axis

    auto none = events_for(&zero, zero);
    CHECK_EQ(none.size(), 1);

    PadReport r;
    r.buttons = kA | kDpadUp | kDpadRight | kRightShoulder;
    r.lx = 1000; r.ly = 32767; r.rx = -32768; r.ry = -32768;
    r.left_trigger = 200; r.right_trigger = 7;
    auto ev1 = events_for(&zero, r);
    CHECK(contains(ev1, kEvKey, 0x130, 1));
    CHECK(contains(ev1, kEvKey, 0x137, 1));
    CHECK(!contains(ev1, kEvKey, 0x131, 1));
    CHECK(contains(ev1, kEvAbs, kAbsHat0X, 1));
    CHECK(contains(ev1, kEvAbs, kAbsHat0Y, -1));
    CHECK(contains(ev1, kEvAbs, kAbsX, 1000));
    CHECK(contains(ev1, kEvAbs, kAbsY, -32768));  // stick up (ly = 32767) is -32768 on the Linux side
    CHECK(contains(ev1, kEvAbs, kAbsRx, -32768));
    CHECK(contains(ev1, kEvAbs, kAbsRy, 32767));
    CHECK(contains(ev1, kEvAbs, kAbsZ, 200));  // left trigger on ABS_Z (xpad layout)
    CHECK(contains(ev1, kEvAbs, kAbsRz, 7));
    // only changed things in the next update
    PadReport r2 = r;
    r2.buttons = kA | kDpadUp;  // right shoulder and D-pad right released
    auto ev2 = events_for(&r, r2);
    CHECK_EQ(ev2.size(), 3);
    CHECK(contains(ev2, kEvKey, 0x137, 0));
    CHECK(contains(ev2, kEvAbs, kAbsHat0X, 0));
    PadReport dl; dl.buttons = kDpadDown | kDpadLeft;
    auto ev3 = events_for(&zero, dl);
    CHECK(contains(ev3, kEvAbs, kAbsHat0X, -1));
    CHECK(contains(ev3, kEvAbs, kAbsHat0Y, 1));
    PadReport g; g.buttons = kGuide;
    CHECK(contains(events_for(&zero, g), kEvKey, 0x13c, 1));
    // the kernel's Y inversion: ~v is a bijection on int16
    for (int v = -32768; v <= 32767; v += 257) {
        PadReport p; p.ly = static_cast<int16_t>(v);
        const auto e = events_for(&zero, p);
        for (const auto& x : e) if (x.type == kEvAbs && x.code == kAbsY) CHECK_EQ(-x.value - 1, v);
    }
    PadReport full; full.buttons = 0xFFFF; full.lx = 1; full.ly = 2; full.rx = 3; full.ry = 4; full.left_trigger = 5; full.right_trigger = 6;
    CHECK(events_for(nullptr, full).size() <= kMaxPadEvents);
}

// A short session through the real mapping: events in, report out.
void test_event_to_report() {
    RawState s;
    MapConfig cfg;
    auto send = [&](std::initializer_list<InputEvent> list) {
        bool report = false;
        for (const auto& e : list) report = s.apply(e) || report;
        return report;
    };
    CHECK(send({ev(kEvAbs, kAbsX, 255), ev(kEvKey, 0x130, 1), ev(kEvSyn, kSynReport, 0)}));
    PadReport r = map_input(s.raw(), cfg);
    CHECK_EQ(r.lx, 32767);
    CHECK_EQ(r.buttons, kA);
    CHECK(send({ev(kEvAbs, kAbsBrake, 100), ev(kEvKey, 0x138, 1), ev(kEvSyn, kSynReport, 0)}));
    r = map_input(s.raw(), cfg);
    CHECK_EQ(r.left_trigger, 100);  // the click does not force 255
    CHECK(send({ev(kEvAbs, kAbsBrake, 0), ev(kEvKey, 0x138, 0), ev(kEvKey, 0x130, 0), ev(kEvAbs, kAbsX, 127), ev(kEvSyn, kSynReport, 0)}));
    r = map_input(s.raw(), cfg);
    CHECK(r == PadReport{});
}

}  // namespace

void test_evdev_logic() {
    test_raw_state();
    test_matcher();
    test_pad_events();
    test_event_to_report();
}
