#include "u2c/platform/linux_service.h"

#include "u2c/axes.h"

#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/timerfd.h>
#include <unistd.h>

#include <cerrno>
#include <chrono>
#include <cstring>

namespace u2c::platform {

namespace {

// Packs the settings the service thread needs into one word, so another thread can hand them over without a lock.
uint32_t pack(const Settings& s) {
    return static_cast<uint32_t>(s.deadzone) | (static_cast<uint32_t>(s.response_curve) << 2) |
           (static_cast<uint32_t>(s.hair_trigger) << 4) | (static_cast<uint32_t>(s.nintendo_mode) << 5) |
           (static_cast<uint32_t>(s.polling_rate) << 6) | (static_cast<uint32_t>(s.exclusive_grab) << 8);
}

void drain_fd(int fd) {
    uint64_t v;
    while (read(fd, &v, sizeof v) > 0) {}
}

}  // namespace

LinuxService::LinuxService(std::unique_ptr<Backend> backend, Options options)
    : backend_(std::move(backend)), options_(options) {
    apply_settings(Settings{});
    settings_dirty_ = false;
}

LinuxService::~LinuxService() { stop(); }

int64_t LinuxService::now_ns() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

void LinuxService::apply_settings(const Settings& settings) {
    packed_settings_ = pack(settings);
    settings_dirty_ = true;
    if (wake_fd_ >= 0) {
        const uint64_t one = 1;
        const ssize_t w = write(wake_fd_, &one, sizeof one);
        (void)w;
    }
}

void LinuxService::start() {
    if (running_) return;
    stop_ = false;
    wake_fd_ = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (wake_fd_ < 0) return;
    running_ = true;
    thread_ = std::thread([this] { run(); });

    // The battery reader runs in its own thread, independent of the controller connection.
    if (auto provider = backend_->create_battery_provider()) {
        BatteryMonitor::Callbacks cb;
        cb.on_started = [this] { emit_log("LogBatteryActive"); };
        cb.on_change = [this](const BatteryInfo& b) {
            {
                std::lock_guard<std::mutex> lock(mutex_);
                state_.battery = b;
            }
            if (b.level >= 0) emit_log("LogBatteryLevel", {{"name", b.device}, {"level", std::to_string(b.level)}});
            if (ServiceObserver* o = observer_.load()) o->on_battery(b);
        };
        cb.on_availability = [this](bool ok) {
            if (ok) { bluetooth_notified_ = false; return; }
            if (!bluetooth_notified_.exchange(true))
                if (ServiceObserver* o = observer_.load()) o->on_notice(Notice{NoticeKind::NoBluetoothService, std::string()});
        };
        battery_ = std::make_unique<BatteryMonitor>(std::move(provider), std::move(cb));
        battery_->start();
    }
}

void LinuxService::stop() {
    if (!running_) return;
    if (battery_) { battery_->stop(); battery_.reset(); }  // first, so no battery event arrives after the stop events
    stop_ = true;
    const uint64_t one = 1;
    const ssize_t w = write(wake_fd_, &one, sizeof one);
    (void)w;
    if (thread_.joinable()) thread_.join();
    close(wake_fd_);
    wake_fd_ = -1;
    running_ = false;
}

ServiceState LinuxService::snapshot() const {
    std::lock_guard<std::mutex> lock(mutex_);
    ServiceState s = state_;
    if (s.status == ServiceStatus::Stopped) return s;
    const uint64_t total = writes_.load();
    meter_.on_writes(static_cast<long>(total - counted_writes_));
    counted_writes_ = total;
    RateMeter::Sample sample;
    if (meter_.poll(now_ns(), sample)) {
        state_.live_hz = sample.hz;
        state_.live_ms = sample.ms;
        s.live_hz = sample.hz;
        s.live_ms = sample.ms;
    }
    return s;
}

void LinuxService::set_state(ServiceStatus status, const std::string& device) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        state_.status = status;
        state_.device_name = status == ServiceStatus::Connected ? device : std::string();
    }
    if (ServiceObserver* o = observer_.load()) o->on_status(status, device);
}

void LinuxService::emit_log(const char* key, std::vector<std::pair<std::string, std::string>> values) {
    if (ServiceObserver* o = observer_.load()) o->on_log(LogEvent{key, std::move(values)});
}

void LinuxService::emit_notice_once(NoticeKind kind, const std::string& detail) {
    const std::string id = std::to_string(static_cast<int>(kind)) + ":" + detail;
    if (!notified_.insert(id).second) return;
    if (ServiceObserver* o = observer_.load()) o->on_notice(Notice{kind, detail});
}

void LinuxService::arm_timer(int fd, int64_t ns) {
    itimerspec its{};
    if (ns > 0) {
        its.it_value.tv_sec = ns / 1000000000LL;
        its.it_value.tv_nsec = ns % 1000000000LL;
    }
    timerfd_settime(fd, 0, &its, nullptr);
}

void LinuxService::reload_settings() {
    const uint32_t p = packed_settings_.load();
    map_config_.deadzone = kDeadzonePresets[p & 3];
    map_config_.curve = static_cast<int>((p >> 2) & 3);
    map_config_.hair_trigger = (p >> 4) & 1;
    map_config_.nintendo_mode = (p >> 5) & 1;
    limiter_.set_rate(kPollingRates[(p >> 6) & 3]);
    const bool grab = (p >> 8) & 1;
    grab_wanted_ = grab;
    if (source_ && grab != grab_active_) {
        if (source_->set_grab(grab)) grab_active_ = grab;
        else if (grab) emit_log("LogGrabFailed");
    }
    if (source_) update_output();
}

void LinuxService::write_report() {
    pad_->write(current_);
    last_written_ = current_;
    have_written_ = true;
    limiter_.mark_written(now_ns());
    ++writes_;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        state_.live_input = current_;
    }
    if (ServiceObserver* o = observer_.load()) o->on_input(current_);
}

void LinuxService::apply_decision(const RateLimiter::Decision& d) {
    if (d.write_now) {
        write_report();
        if (rate_timer_armed_) { arm_timer(rate_timer_fd_, 0); rate_timer_armed_ = false; }
    } else if (d.wait_ns > 0) {
        arm_timer(rate_timer_fd_, d.wait_ns);
        rate_timer_armed_ = true;
    }
}

void LinuxService::update_output() {
    current_ = map_input(raw_.raw(), map_config_);
    if (have_written_ && current_ == last_written_) {  // the value went back to what was written
        limiter_.clear_pending();
        if (rate_timer_armed_) { arm_timer(rate_timer_fd_, 0); rate_timer_armed_ = false; }
        return;
    }
    apply_decision(limiter_.on_change(now_ns()));
}

void LinuxService::handle_rate_timer() {
    drain_fd(rate_timer_fd_);
    rate_timer_armed_ = false;
    if (!source_) return;
    if (have_written_ && current_ == last_written_) { limiter_.clear_pending(); return; }
    apply_decision(limiter_.on_timer(now_ns()));
}

void LinuxService::handle_source() {
    bool dirty = false;
    InputEvent buf[64];
    for (;;) {
        const int n = source_->read_events(buf, 64);
        if (n < 0) { disconnect_device(); return; }
        if (n == 0) break;
        for (int i = 0; i < n; ++i)
            if (raw_.apply(buf[i])) dirty = true;
    }
    if (raw_.take_dropped()) {  // the kernel dropped events: read the whole state again
        std::vector<InputEvent> state;
        if (!source_->read_state(state)) { disconnect_device(); return; }
        for (const auto& e : state) raw_.apply(e);
        dirty = true;
    }
    if (dirty) update_output();
}

void LinuxService::disconnect_device() {
    if (rate_timer_armed_) { arm_timer(rate_timer_fd_, 0); rate_timer_armed_ = false; }
    if (pad_) {
        if (have_written_ && !(last_written_ == PadReport{})) pad_->write(PadReport{});  // release everything first
        pad_.reset();
    }
    if (source_) {
        epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, source_->fd(), nullptr);
        source_.reset();  // closing the device also drops the exclusive grab
    }
    grab_active_ = false;
    have_written_ = false;
    last_written_ = PadReport{};
    notified_.clear();
    emit_log("LogMapperDisconnected");
    {
        std::lock_guard<std::mutex> lock(mutex_);
        state_.live_input = PadReport{};
    }
    if (ServiceObserver* o = observer_.load()) o->on_input(PadReport{});
    set_state(ServiceStatus::Disconnected, std::string());
    set_state(ServiceStatus::Searching, std::string());
}

void LinuxService::try_connect() {
    std::vector<Notice> problems;
    const std::vector<Candidate> candidates = backend_->scan(problems);
    for (const Notice& n : problems) emit_notice_once(n.kind, n.detail);

    for (const Candidate& c : candidates) {
        int err = 0;
        std::unique_ptr<InputSource> source = backend_->open(c, &err);
        if (!source) {
            if (err == EACCES || err == EPERM) emit_notice_once(NoticeKind::NoControllerAccess, c.path);
            continue;
        }
        std::vector<InputEvent> state;
        if (!source->read_state(state)) continue;

        std::unique_ptr<PadSink> pad = backend_->create_pad(&err);
        if (!pad) {
            emit_notice_once(NoticeKind::NoVirtualDevice, "/dev/uinput");
            arm_timer(retry_timer_fd_, static_cast<int64_t>(options_.retry_ms) * 1000000LL);
            return;  // closed again, we try later
        }

        raw_.reset();
        for (const auto& e : state) raw_.apply(e);
        grab_active_ = false;
        if (grab_wanted_) {
            grab_active_ = source->set_grab(true);
            if (!grab_active_) emit_log("LogGrabFailed");
        }
        epoll_event ev{};
        ev.events = EPOLLIN;
        ev.data.fd = source->fd();
        epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, source->fd(), &ev);
        source_ = std::move(source);
        pad_ = std::move(pad);
        have_written_ = false;
        limiter_.reset();
        notified_.clear();
        arm_timer(retry_timer_fd_, 0);

        const std::string name = source_->name();
        emit_log("LogControllerConnected", {{"name", name}});
        set_state(ServiceStatus::Connected, name);
        update_output();
        return;
    }
}

void LinuxService::run() {
    emit_log("LogServicesStarting");
    emit_log("LogMapperStart");
    set_state(ServiceStatus::Searching, std::string());

    epoll_fd_ = epoll_create1(EPOLL_CLOEXEC);
    rate_timer_fd_ = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
    retry_timer_fd_ = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
    const int watch_fd = backend_->watch_fd();
    for (int fd : {wake_fd_, rate_timer_fd_, retry_timer_fd_, watch_fd}) {
        if (fd < 0) continue;
        epoll_event ev{};
        ev.events = EPOLLIN;
        ev.data.fd = fd;
        epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fd, &ev);
    }
    reload_settings();
    settings_dirty_ = false;

    while (!stop_) {
        if (!source_) try_connect();
        epoll_event events[8];
        const int n = epoll_wait(epoll_fd_, events, 8, -1);
        if (n < 0) { if (errno == EINTR) continue; break; }
        for (int i = 0; i < n && !stop_; ++i) {
            const int fd = events[i].data.fd;
            if (fd == wake_fd_) {
                drain_fd(wake_fd_);
                if (settings_dirty_.exchange(false)) reload_settings();
            } else if (fd == watch_fd) {
                backend_->drain_watch();
            } else if (fd == retry_timer_fd_) {
                drain_fd(retry_timer_fd_);
            } else if (fd == rate_timer_fd_) {
                handle_rate_timer();
            } else if (source_ && fd == source_->fd()) {
                if (events[i].events & (EPOLLERR | EPOLLHUP)) disconnect_device();
                else handle_source();
            }
        }
    }

    // stopping: release the pad, drop the device, tell the observer
    if (rate_timer_armed_) arm_timer(rate_timer_fd_, 0);
    if (pad_) {
        if (have_written_ && !(last_written_ == PadReport{})) pad_->write(PadReport{});
        pad_.reset();
    }
    if (source_) { epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, source_->fd(), nullptr); source_.reset(); }
    emit_log("LogServicesStopping");
    {
        std::lock_guard<std::mutex> lock(mutex_);
        state_ = ServiceState{};
    }
    if (ServiceObserver* o = observer_.load()) {
        o->on_input(PadReport{});
        o->on_battery(BatteryInfo{});
        o->on_status(ServiceStatus::Stopped, std::string());
    }
    close(retry_timer_fd_);
    close(rate_timer_fd_);
    close(epoll_fd_);
    retry_timer_fd_ = rate_timer_fd_ = epoll_fd_ = -1;
    have_written_ = false;
    last_written_ = PadReport{};
    notified_.clear();
}

}  // namespace u2c::platform
