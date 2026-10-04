// Tiny test runner, no framework. Exit code 0 only if every check passes.
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>

#include "check.h"
#include "u2c/axes.h"
#include "u2c/buttons.h"
#include "u2c/pad.h"
#include "u2c/triggers.h"

using namespace u2c;

namespace {
// CTest sets U2C_VECTOR_DIR. Run from the repository folder without it, the default below is used.
std::string vector_dir() {
    const char* dir = std::getenv("U2C_VECTOR_DIR");
    return dir && dir[0] ? dir : "core/tests/vectors";
}

// Reads a CSV with a header line and calls fn(fields) for every row. Returns the row count, or -1 if the file is missing.
template <int N, typename Fn>
long for_each_row(const std::string& name, Fn fn) {
    std::ifstream in(vector_dir() + "/" + name);
    if (!in) { std::printf("FAIL cannot open %s/%s\n", vector_dir().c_str(), name.c_str()); ++u2ctest::g_failures; return -1; }
    std::string line;
    std::getline(in, line);
    long rows = 0;
    while (std::getline(in, line)) {
        long long f[N];
        int pos = 0;
        const char* s = line.c_str();
        for (int i = 0; i < N; ++i) {
            char* end = nullptr;
            f[i] = std::strtoll(s + pos, &end, 10);
            pos = static_cast<int>(end - s) + 1;
        }
        fn(f);
        ++rows;
    }
    return rows;
}

void test_known_values() {
    CHECK_EQ(normalize_axis(0), -32767);
    CHECK_EQ(normalize_axis(32767), 0);
    CHECK_EQ(normalize_axis(65535), 32767);
    CHECK_EQ(negate_axis(-32768), 32767);
    CHECK_EQ(negate_axis(100), -100);
    CHECK_EQ(apply_deadzone(3999, 4000), 0);
    CHECK_EQ(apply_deadzone(4000, 4000), 4000);
    CHECK_EQ(apply_deadzone(-3999, 4000), 0);
    CHECK_EQ(apply_deadzone(-4000, 4000), -4000);
    CHECK_EQ(apply_deadzone(5, 0), 5);
    CHECK_EQ(apply_curve(32767, 1), 32767);
    CHECK_EQ(apply_curve(32767, 2), 32767);
    CHECK_EQ(apply_curve(-32768, 1), -32767);  // truncation of the clamped -1.0000305 value, as in the Windows code
    CHECK_EQ(apply_curve(0, 2), 0);
    CHECK_EQ(scale_8_to_16(0), 0);
    CHECK_EQ(scale_8_to_16(255), 65535);
    // Rest value 127 is slightly off centre on this 8-bit controller (same as the Windows formulas).
    CHECK_EQ(stick_axis(scale_8_to_16(127), 0, 0), -128);
    CHECK_EQ(stick_axis(scale_8_to_16(127), 4000, 0), 0);  // inside the default dead zone
    CHECK_EQ(stick_axis(scale_8_to_16(128), 0, 0), 129);
}

void test_triggers() {
    // threshold edge: a difference of 1499 is zero, 1500 is not
    CHECK_EQ(trigger_windows(1499, 0, false, false), 0);
    CHECK_EQ(trigger_windows(1500, 0, false, false), 5);
    CHECK_EQ(trigger_windows(1500, 0, false, true), 255);
    // Linux: idle 0, 8-bit input, the result equals the raw value above the threshold (257 * 255 == 65535)
    CHECK_EQ(trigger_analog(scale_8_to_16(0), 0, false, false, true), 0);
    CHECK_EQ(trigger_analog(scale_8_to_16(5), 0, false, false, true), 0);
    CHECK_EQ(trigger_analog(scale_8_to_16(6), 0, false, false, true), 6);
    CHECK_EQ(trigger_analog(scale_8_to_16(100), 0, false, false, true), 100);
    CHECK_EQ(trigger_analog(scale_8_to_16(255), 0, false, false, true), 255);
    CHECK_EQ(trigger_analog(scale_8_to_16(5), 0, false, true, true), 0);
    CHECK_EQ(trigger_analog(scale_8_to_16(6), 0, false, true, true), 255);
    // the click must not force 255 when the analog value is present
    CHECK_EQ(trigger_analog(scale_8_to_16(40), 0, true, false, true), 40);
    CHECK_EQ(trigger_windows(scale_8_to_16(40), 0, true, false), 255);  // Windows behavior, kept for comparison
    // fallback without an analog axis
    CHECK_EQ(trigger_analog(0, 0, true, false, false), 255);
    CHECK_EQ(trigger_analog(0, 0, false, false, false), 0);
}

void test_buttons() {
    CHECK_EQ(key_from_evdev_code(0x130), kKeySouth);
    CHECK_EQ(key_from_evdev_code(0x13e), kKeyThumbR);
    CHECK_EQ(key_from_evdev_code(0x13f), 15);
    CHECK_EQ(key_from_evdev_code(0x12f), -1);
    CHECK_EQ(key_from_evdev_code(0x140), -1);
    CHECK_EQ(map_buttons(key_bit(kKeySouth), false), kA);
    CHECK_EQ(map_buttons(key_bit(kKeyEast), false), kB);
    CHECK_EQ(map_buttons(key_bit(kKeyNorth), false), kX);
    CHECK_EQ(map_buttons(key_bit(kKeyWest), false), kY);
    CHECK_EQ(map_buttons(key_bit(kKeySouth), true), kB);  // Nintendo Mode swaps A/B and X/Y
    CHECK_EQ(map_buttons(key_bit(kKeyEast), true), kA);
    CHECK_EQ(map_buttons(key_bit(kKeyNorth), true), kY);
    CHECK_EQ(map_buttons(key_bit(kKeyWest), true), kX);
    CHECK_EQ(map_buttons(key_bit(kKeyTl), true), kLeftShoulder);
    CHECK_EQ(map_buttons(key_bit(kKeySelect), false), kBack);
    CHECK_EQ(map_buttons(key_bit(kKeyStart), false), kStart);
    CHECK_EQ(map_buttons(key_bit(kKeyThumbL), false), kLeftThumb);
    CHECK_EQ(map_buttons(key_bit(kKeyThumbR), false), kRightThumb);
    // not mapped: click keys, Mode, C, Z
    CHECK_EQ(map_buttons(key_bit(kKeyTl2) | key_bit(kKeyTr2) | key_bit(kKeyMode) | key_bit(kKeyC) | key_bit(kKeyZ), false), 0);
    CHECK_EQ(dpad_from_hat(0, 0), 0);
    CHECK_EQ(dpad_from_hat(0, -1), kDpadUp);
    CHECK_EQ(dpad_from_hat(1, 0), kDpadRight);
    CHECK_EQ(dpad_from_hat(0, 1), kDpadDown);
    CHECK_EQ(dpad_from_hat(-1, 0), kDpadLeft);
    CHECK_EQ(dpad_from_hat(1, -1), kDpadUp | kDpadRight);
    CHECK_EQ(dpad_from_hat(-1, 1), kDpadDown | kDpadLeft);
}

void test_pipeline() {
    RawInput rest;
    MapConfig cfg;
    PadReport r = map_input(rest, cfg);
    CHECK(r == PadReport{});  // a resting controller gives an all-zero report

    RawInput in;
    in.abs_x = 255; in.abs_y = 0; in.abs_z = 0; in.abs_rz = 255;
    in.abs_brake = 255; in.abs_gas = 100;
    in.hat_y = -1;
    in.keys = key_bit(kKeySouth) | key_bit(kKeyTl2);
    r = map_input(in, cfg);
    CHECK_EQ(r.lx, 32767);
    CHECK_EQ(r.ly, 32767);
    CHECK_EQ(r.rx, -32767);
    CHECK_EQ(r.ry, -32767);
    CHECK_EQ(r.left_trigger, 255);
    CHECK_EQ(r.right_trigger, 100);
    CHECK_EQ(r.buttons, kA | kDpadUp);

    in.keys = key_bit(kKeyTl2);  // click alone, trigger axis at 40: the analog value wins
    in.abs_brake = 40;
    r = map_input(in, cfg);
    CHECK_EQ(r.left_trigger, 40);
}

void test_vectors() {
    long n = for_each_row<5>("axes.csv", [](const long long* f) {
        const int32_t raw = static_cast<int32_t>(f[0]);
        const int dz = static_cast<int>(f[1]), curve = static_cast<int>(f[2]);
        CHECK_EQ(stick_axis(raw, dz, curve), f[3]);
        CHECK_EQ(stick_axis_inverted(raw, dz, curve), f[4]);
    });
    std::printf("axes.csv: %ld rows\n", n);
    n = for_each_row<5>("triggers_windows.csv", [](const long long* f) {
        CHECK_EQ(trigger_windows(static_cast<int32_t>(f[0]), static_cast<int32_t>(f[1]), f[2] != 0, f[3] != 0), f[4]);
    });
    std::printf("triggers_windows.csv: %ld rows\n", n);
    n = for_each_row<5>("triggers_analog.csv", [](const long long* f) {
        CHECK_EQ(trigger_analog(scale_8_to_16(static_cast<int>(f[0])), 0, f[1] != 0, f[2] != 0, f[3] != 0), f[4]);
    });
    std::printf("triggers_analog.csv: %ld rows\n", n);
}
}  // namespace

int main() {
    test_known_values();
    test_triggers();
    test_buttons();
    test_pipeline();
    test_vectors();
    test_settings();
    test_translations();
    test_rate_limiter();
    std::printf("checks: %ld, failures: %ld\n", u2ctest::g_checks, u2ctest::g_failures);
    return u2ctest::g_failures == 0 ? 0 : 1;
}
