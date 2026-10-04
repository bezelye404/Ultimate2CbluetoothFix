// What the Linux service needs from the system, as interfaces. The real implementation is in linux_backend.h;
// tests use fakes, so the service logic runs without a controller.
#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "u2c/pad.h"
#include "u2c/service.h"

namespace u2c::platform {

struct InputEvent {
    uint16_t type = 0;
    uint16_t code = 0;
    int32_t value = 0;
};

class InputSource {
public:
    virtual ~InputSource() = default;
    virtual int fd() const = 0;  // for epoll; readable when events are waiting
    virtual std::string name() const = 0;
    // Reads waiting events without blocking. Returns how many were read (0 = none), or -1 when the device is gone.
    virtual int read_events(InputEvent* buf, int capacity) = 0;
    // Current state as events. Used at start and after SYN_DROPPED. False on failure.
    virtual bool read_state(std::vector<InputEvent>& out) = 0;
    // Exclusive access: other programs stop seeing this device. False if it could not be done.
    virtual bool set_grab(bool on) = 0;
};

class PadSink {
public:
    virtual ~PadSink() = default;
    virtual bool write(const PadReport& report) = 0;
};

struct Candidate {
    std::string path;  // device node of a matching controller
};

// Source of the battery level. Runs in its own thread (BatteryMonitor), so it may block.
class BatteryProvider {
public:
    virtual ~BatteryProvider() = default;
    virtual void start() = 0;
    virtual int fd() const = 0;  // to poll, -1 if none right now
    virtual short poll_events() const = 0;
    virtual int timeout_ms() const = 0;  // -1 = no timeout
    virtual void process() = 0;  // called when fd() is ready or the timeout passed
    virtual BatteryInfo current() const = 0;  // level -1 when unknown
    virtual bool available() const = 0;  // false when the Bluetooth service cannot be reached
};

class Backend {
public:
    virtual ~Backend() = default;
    // Becomes readable when devices may have appeared or changed (inotify); -1 if there is none.
    virtual int watch_fd() = 0;
    virtual void drain_watch() = 0;  // empties it after it became readable
    // Finds matching controllers. `problems` receives things the user can fix (such as a missing permission).
    virtual std::vector<Candidate> scan(std::vector<Notice>& problems) = 0;
    virtual std::unique_ptr<InputSource> open(const Candidate& candidate, int* err) = 0;
    virtual std::unique_ptr<PadSink> create_pad(int* err) = 0;  // err: errno
    // nullptr if this backend has no battery source
    virtual std::unique_ptr<BatteryProvider> create_battery_provider() { return nullptr; }
};

}  // namespace u2c::platform
