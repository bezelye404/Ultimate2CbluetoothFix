// The real backend: evdev devices in /dev/input, a virtual Xbox 360 pad through /dev/uinput, inotify for hot plugging.
// Kernel headers only, no extra library.
#pragma once
#include <string>

#include "u2c/platform/backend.h"

namespace u2c::platform {

struct LinuxBackendPaths {
    std::string input_dir = "/dev/input";
    std::string sys_class_input = "/sys/class/input";  // read for the vendor id of each event device
    std::string uinput = "/dev/uinput";
};

class LinuxBackend : public Backend {
public:
    explicit LinuxBackend(LinuxBackendPaths paths = {});
    ~LinuxBackend() override;
    LinuxBackend(const LinuxBackend&) = delete;
    LinuxBackend& operator=(const LinuxBackend&) = delete;

    int watch_fd() override { return inotify_fd_; }
    void drain_watch() override;
    std::vector<Candidate> scan(std::vector<Notice>& problems) override;
    std::unique_ptr<InputSource> open(const Candidate& candidate, int* err) override;
    std::unique_ptr<PadSink> create_pad(int* err) override;
    std::unique_ptr<BatteryProvider> create_battery_provider() override;

private:
    LinuxBackendPaths paths_;
    int inotify_fd_ = -1;
};

}  // namespace u2c::platform
