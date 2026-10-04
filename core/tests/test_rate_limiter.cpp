#include <algorithm>
#include <cstdint>
#include <vector>

#include "check.h"
#include "u2c/rate_limiter.h"

using namespace u2c;

namespace {

constexpr int64_t kMs = 1000000;

void test_basic_sequence() {
    RateLimiter rl(250);
    CHECK_EQ(rl.interval_ns(), 4 * kMs);
    CHECK(!rl.pending());
    CHECK(rl.on_timer(0).write_now == false && rl.on_timer(0).wait_ns == 0);  // idle: nothing to do, no timer

    auto d = rl.on_change(0);  // first change is written at once
    CHECK(d.write_now);
    rl.mark_written(0);
    d = rl.on_change(1 * kMs);  // too early: wait 3 ms
    CHECK(!d.write_now);
    CHECK_EQ(d.wait_ns, 3 * kMs);
    CHECK(rl.pending());
    d = rl.on_change(2 * kMs);  // another change: still the same deadline
    CHECK(!d.write_now);
    CHECK_EQ(d.wait_ns, 2 * kMs);
    d = rl.on_timer(3 * kMs);  // timer fired a little early: wait the rest
    CHECK(!d.write_now);
    CHECK_EQ(d.wait_ns, 1 * kMs);
    d = rl.on_timer(4 * kMs);
    CHECK(d.write_now);
    rl.mark_written(4 * kMs);
    CHECK(!rl.pending());
    d = rl.on_timer(8 * kMs);  // nothing pending: no write, no timer
    CHECK(!d.write_now && d.wait_ns == 0);
    d = rl.on_change(100 * kMs);  // after a long pause: written at once again
    CHECK(d.write_now);

    rl.reset();  // a new connection starts fresh
    CHECK(rl.on_change(5).write_now);
}

void test_clear_pending() {
    RateLimiter rl(250);
    rl.mark_written(0);
    CHECK_EQ(rl.on_change(1 * kMs).wait_ns, 3 * kMs);
    CHECK(rl.pending());
    rl.clear_pending();  // the value went back to what was written
    CHECK(!rl.pending());
    const auto d = rl.on_timer(4 * kMs);
    CHECK(!d.write_now && d.wait_ns == 0);
}

void test_rate_change() {
    RateLimiter rl(125);
    CHECK_EQ(rl.interval_ns(), 8 * kMs);
    rl.mark_written(0);
    CHECK_EQ(rl.on_change(1 * kMs).wait_ns, 7 * kMs);
    rl.set_rate(1000);  // a faster setting applies at the next decision
    CHECK_EQ(rl.interval_ns(), 1 * kMs);
    CHECK(rl.on_timer(1 * kMs).write_now);
    rl.set_rate(0);  // nonsense input falls back to one second, no crash
    CHECK_EQ(rl.interval_ns(), 1000 * kMs);
}

// Simulation: random changes and one timer; checks the guarantees of the header for every rate.
void simulate(int hz, uint32_t seed, int64_t max_gap_ns, int count) {
    RateLimiter rl(hz);
    const int64_t interval = rl.interval_ns();
    uint32_t rng = seed;
    auto next_gap = [&] { rng = rng * 1664525u + 1013904223u; return static_cast<int64_t>(rng % static_cast<uint32_t>(max_gap_ns)) + 1; };

    std::vector<int64_t> writes;
    int64_t t = 0, next_event = next_gap();
    int64_t timer = -1;
    int64_t oldest_unwritten = -1;
    int64_t worst_delay = 0;
    int events = 0;
    auto handle = [&](RateLimiter::Decision d, int64_t now) {
        if (d.write_now) {
            if (oldest_unwritten >= 0) worst_delay = std::max(worst_delay, now - oldest_unwritten);
            oldest_unwritten = -1;
            writes.push_back(now);
            rl.mark_written(now);
            timer = -1;
        } else if (d.wait_ns > 0) {
            timer = now + d.wait_ns;
        } else {
            timer = -1;
        }
    };
    while (events < count || timer >= 0) {
        const bool event_next = events < count && (timer < 0 || next_event <= timer);
        if (event_next) {
            t = next_event;
            if (oldest_unwritten < 0) oldest_unwritten = t;
            handle(rl.on_change(t), t);
            ++events;
            next_event = t + next_gap();
        } else {
            t = timer;
            timer = -1;
            handle(rl.on_timer(t), t);
        }
    }
    for (size_t i = 1; i < writes.size(); ++i) CHECK(writes[i] - writes[i - 1] >= interval);
    CHECK(worst_delay <= interval);
    CHECK(!rl.pending());
    CHECK(oldest_unwritten < 0);
    CHECK(static_cast<int>(writes.size()) <= count);
}

void test_simulations() {
    for (int hz : {125, 250, 500, 1000}) {
        for (uint32_t seed = 1; seed <= 20; ++seed) {
            simulate(hz, seed, 100000, 3000);
            simulate(hz, seed, 3 * kMs, 3000);
            simulate(hz, seed, 50 * kMs, 500);
        }
    }
    // 1000 changes within one second at 125 Hz give at most 125 writes (+1 for the first)
    RateLimiter rl(125);
    int writes = 0;
    int64_t timer = -1;
    for (int i = 0; i < 1000; ++i) {
        const int64_t now = static_cast<int64_t>(i) * kMs;
        while (timer >= 0 && timer <= now) {
            const int64_t fire = timer;
            timer = -1;
            const auto d = rl.on_timer(fire);
            if (d.write_now) { rl.mark_written(fire); ++writes; }
            else if (d.wait_ns > 0) timer = fire + d.wait_ns;
        }
        const auto d = rl.on_change(now);
        if (d.write_now) { rl.mark_written(now); ++writes; timer = -1; }
        else if (d.wait_ns > 0 && timer < 0) timer = now + d.wait_ns;
    }
    CHECK(writes >= 124 && writes <= 126);
}

void test_meter() {
    RateMeter m;
    RateMeter::Sample s;
    CHECK(!m.poll(0, s));
    for (int i = 0; i < 250; ++i) m.on_write();
    CHECK(!m.poll(999 * kMs, s));
    CHECK(m.poll(1000 * kMs, s));
    CHECK_EQ(s.hz, 250);
    CHECK(s.ms > 3.99f && s.ms < 4.01f);
    CHECK(!m.poll(1500 * kMs, s));
    CHECK(m.poll(2000 * kMs, s));
    CHECK_EQ(s.hz, 0);
    CHECK(s.ms == 0.0f);
    for (int i = 0; i < 100; ++i) m.on_write();
    CHECK(m.poll(4000 * kMs, s));  // a 2 second window: 100 writes = 50 Hz
    CHECK_EQ(s.hz, 50);
    m.reset();
    CHECK(!m.poll(5000 * kMs, s));
}

}  // namespace

void test_rate_limiter() {
    test_basic_sequence();
    test_clear_pending();
    test_rate_change();
    test_simulations();
    test_meter();
}
