// Runs a BatteryProvider in its own thread and reports changes. The thread sleeps in poll() until the provider's
// descriptor is ready, its timeout passes or stop() is called.
#pragma once
#include <atomic>
#include <functional>
#include <memory>
#include <thread>

#include "u2c/platform/backend.h"

namespace u2c::platform {

class BatteryMonitor {
public:
    struct Callbacks {
        std::function<void()> on_started;  // the thread is running (before the first query)
        std::function<void(const BatteryInfo&)> on_change;  // level or device changed (also the first value)
        std::function<void(bool)> on_availability;  // false: the Bluetooth service cannot be reached; true: back
    };
    BatteryMonitor(std::unique_ptr<BatteryProvider> provider, Callbacks callbacks);
    ~BatteryMonitor();
    void start();
    void stop();

private:
    void run();

    std::unique_ptr<BatteryProvider> provider_;
    Callbacks cb_;
    std::thread thread_;
    std::atomic<bool> stop_{false};
    int stop_fd_ = -1;
};

}  // namespace u2c::platform
