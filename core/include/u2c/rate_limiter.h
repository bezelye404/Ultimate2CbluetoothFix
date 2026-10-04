// Limits how often the virtual pad is written. "Polling Rate" is a maximum update rate.
// Pure decision logic: the caller owns the clock and the timer and passes timestamps in nanoseconds.
// Rules:
//  - A change is written at once if one interval has passed since the last write.
//  - Otherwise it waits, and the caller arms one timer for the remaining time. A newer change replaces
//    the waiting one, so updates never pile up.
//  - Nothing waiting means no timer, so nothing runs while the controller is idle.
#pragma once
#include <cstdint>

namespace u2c {

class RateLimiter {
public:
    struct Decision {
        bool write_now = false;  // write the current state, then call mark_written(now)
        int64_t wait_ns = 0;  // > 0: arm a timer for this long and call on_timer() when it fires
    };

    explicit RateLimiter(int hz = 250) { set_rate(hz); }

    // Any value >= 1 works; takes effect at the next decision.
    void set_rate(int hz) { interval_ns_ = hz >= 1 ? 1000000000LL / hz : 1000000000LL; }
    int64_t interval_ns() const { return interval_ns_; }

    Decision on_change(int64_t now_ns) { return decide(now_ns, true); }
    Decision on_timer(int64_t now_ns) { return decide(now_ns, false); }
    void mark_written(int64_t now_ns) {
        last_write_ns_ = now_ns;
        have_last_ = true;
        pending_ = false;
    }
    bool pending() const { return pending_; }
    // The waiting change equals what was already written (the value went back): nothing to do.
    void clear_pending() { pending_ = false; }
    void reset() { have_last_ = false; pending_ = false; }  // for a new connection

private:
    Decision decide(int64_t now_ns, bool changed) {
        if (changed) pending_ = true;
        if (!pending_) return {};
        if (!have_last_ || now_ns - last_write_ns_ >= interval_ns_) return {true, 0};
        return {false, last_write_ns_ + interval_ns_ - now_ns};
    }

    int64_t interval_ns_ = 4000000;
    int64_t last_write_ns_ = 0;
    bool have_last_ = false;
    bool pending_ = false;
};

// Measured update rate for the "Hz / ms" readout: writes counted over at least one second.
class RateMeter {
public:
    struct Sample {
        int hz = 0;
        float ms = 0.0f;
    };
    void on_write() { ++count_; }
    void on_writes(long n) { count_ += n; }
    // Returns true and fills `out` once a full second has passed.
    bool poll(int64_t now_ns, Sample& out) {
        if (!started_) { started_ = true; window_start_ns_ = now_ns; count_ = 0; return false; }
        const int64_t elapsed = now_ns - window_start_ns_;
        if (elapsed < 1000000000LL) return false;
        out.hz = static_cast<int>((static_cast<double>(count_) * 1e9) / static_cast<double>(elapsed));
        out.ms = out.hz > 0 ? 1000.0f / static_cast<float>(out.hz) : 0.0f;
        window_start_ns_ = now_ns;
        count_ = 0;
        return true;
    }
    void reset() { started_ = false; count_ = 0; }

private:
    bool started_ = false;
    int64_t window_start_ns_ = 0;
    long count_ = 0;
};

}  // namespace u2c
