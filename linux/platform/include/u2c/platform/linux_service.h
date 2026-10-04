// The Linux implementation of Service. Device and pad access goes through a Backend, so searching, connecting,
// rate limiting and disconnecting are tested with fake backends.
#pragma once
#include <atomic>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "u2c/platform/backend.h"
#include "u2c/platform/battery_monitor.h"
#include "u2c/platform/evdev_logic.h"
#include "u2c/rate_limiter.h"
#include "u2c/service.h"

namespace u2c::platform {

struct LinuxServiceOptions {
    int retry_ms = 5000;  // wait before retrying when the virtual pad could not be created
};

class LinuxService : public Service {
public:
    using Options = LinuxServiceOptions;
    explicit LinuxService(std::unique_ptr<Backend> backend, Options options = {});
    ~LinuxService() override;

    void set_observer(ServiceObserver* observer) override { observer_ = observer; }
    void start() override;
    void stop() override;
    bool running() const override { return running_; }
    void apply_settings(const Settings& settings) override;
    ServiceState snapshot() const override;

private:
    void run();
    void try_connect();
    void disconnect_device();
    void handle_source();
    void handle_rate_timer();
    void update_output();
    void apply_decision(const RateLimiter::Decision& d);
    void write_report();
    void reload_settings();
    void arm_timer(int fd, int64_t ns);
    void set_state(ServiceStatus status, const std::string& device);
    void emit_log(const char* key, std::vector<std::pair<std::string, std::string>> values = {});
    void emit_notice_once(NoticeKind kind, const std::string& detail);
    static int64_t now_ns();

    std::unique_ptr<Backend> backend_;
    Options options_;
    std::atomic<ServiceObserver*> observer_{nullptr};
    std::thread thread_;
    bool running_ = false;

    // written by other threads
    int wake_fd_ = -1;
    std::atomic<bool> stop_{false};
    std::atomic<bool> settings_dirty_{false};
    std::atomic<uint32_t> packed_settings_{0};
    std::atomic<uint64_t> writes_{0};
    std::atomic<bool> bluetooth_notified_{false};
    std::unique_ptr<BatteryMonitor> battery_;

    // service thread only
    int epoll_fd_ = -1, rate_timer_fd_ = -1, retry_timer_fd_ = -1;
    bool rate_timer_armed_ = false;
    std::unique_ptr<InputSource> source_;
    std::unique_ptr<PadSink> pad_;
    RawState raw_;
    PadReport current_, last_written_;
    bool have_written_ = false;
    RateLimiter limiter_;
    MapConfig map_config_;
    bool grab_wanted_ = true;
    bool grab_active_ = false;
    std::set<std::string> notified_;

    // shared with snapshot()
    mutable std::mutex mutex_;
    mutable ServiceState state_;
    mutable RateMeter meter_;
    mutable uint64_t counted_writes_ = 0;
};

}  // namespace u2c::platform
