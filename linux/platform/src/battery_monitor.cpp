#include "u2c/platform/battery_monitor.h"

#include <poll.h>
#include <sys/eventfd.h>
#include <unistd.h>

namespace u2c::platform {

BatteryMonitor::BatteryMonitor(std::unique_ptr<BatteryProvider> provider, Callbacks callbacks)
    : provider_(std::move(provider)), cb_(std::move(callbacks)) {}

BatteryMonitor::~BatteryMonitor() { stop(); }

void BatteryMonitor::start() {
    if (thread_.joinable() || !provider_) return;
    stop_ = false;
    stop_fd_ = eventfd(0, EFD_CLOEXEC);
    if (stop_fd_ < 0) return;
    thread_ = std::thread([this] { run(); });
}

void BatteryMonitor::stop() {
    if (!thread_.joinable()) return;
    stop_ = true;
    const uint64_t one = 1;
    const ssize_t w = write(stop_fd_, &one, sizeof one);
    (void)w;
    thread_.join();
    close(stop_fd_);
    stop_fd_ = -1;
}

void BatteryMonitor::run() {
    if (cb_.on_started) cb_.on_started();
    provider_->start();
    bool available = provider_->available();
    BatteryInfo last = provider_->current();
    if (!available && cb_.on_availability) cb_.on_availability(false);
    if (last.level >= 0 && cb_.on_change) cb_.on_change(last);

    while (!stop_) {
        pollfd fds[2];
        nfds_t n = 0;
        fds[n++] = {stop_fd_, POLLIN, 0};
        if (provider_->fd() >= 0) fds[n++] = {provider_->fd(), provider_->poll_events(), 0};
        const int ready = poll(fds, n, provider_->timeout_ms());
        if (stop_) break;
        if (ready < 0) continue;
        provider_->process();

        const bool now_available = provider_->available();
        if (now_available != available) {
            available = now_available;
            if (cb_.on_availability) cb_.on_availability(available);
        }
        const BatteryInfo cur = provider_->current();
        if (!(cur == last)) {
            last = cur;
            if (cb_.on_change) cb_.on_change(cur);
        }
    }
}

}  // namespace u2c::platform
