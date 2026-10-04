#include "u2c/platform/linux_backend.h"

#include <dirent.h>
#include <fcntl.h>
#include <linux/input.h>
#include <linux/uinput.h>
#include <sys/inotify.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <array>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "u2c/platform/bluez_battery.h"
#include "u2c/platform/evdev_logic.h"

namespace u2c::platform {

namespace {

bool bit_set(const unsigned char* bits, int bit) { return (bits[bit / 8] >> (bit % 8)) & 1; }

constexpr uint16_t kStateAbs[] = {kAbsX, kAbsY, kAbsZ, kAbsRz, kAbsGas, kAbsBrake, kAbsHat0X, kAbsHat0Y};

struct KeyBits {
    unsigned char bits[(KEY_MAX + 8) / 8] = {};
};
bool key_present(void* ctx, uint16_t code) { return bit_set(static_cast<KeyBits*>(ctx)->bits, code); }

// Reads identity and capabilities of an open event device and decides whether it is the controller.
bool device_matches(int fd) {
    input_id id{};
    if (ioctl(fd, EVIOCGID, &id) != 0) return false;
    std::array<AbsInfo, 0x12> abs{};
    unsigned char abs_bits[(ABS_MAX + 8) / 8] = {};
    if (ioctl(fd, EVIOCGBIT(EV_ABS, sizeof abs_bits), abs_bits) < 0) return false;
    for (uint16_t code = 0; code < abs.size(); ++code) {
        if (!bit_set(abs_bits, code)) continue;
        input_absinfo info{};
        if (ioctl(fd, EVIOCGABS(code), &info) != 0) return false;
        abs[code] = {true, info.minimum, info.maximum};
    }
    KeyBits keys;
    if (ioctl(fd, EVIOCGBIT(EV_KEY, sizeof keys.bits), keys.bits) < 0) return false;
    return matches_controller(id.vendor, abs, key_present, &keys);
}

bool read_vendor(const std::string& sys_dir, const std::string& name, unsigned& vendor) {
    const std::string path = sys_dir + "/" + name + "/device/id/vendor";
    FILE* f = std::fopen(path.c_str(), "r");
    if (!f) return false;
    const int n = std::fscanf(f, "%x", &vendor);
    std::fclose(f);
    return n == 1;
}

class LinuxSource : public InputSource {
public:
    LinuxSource(int fd, std::string name) : fd_(fd), name_(std::move(name)) {}
    ~LinuxSource() override { if (fd_ >= 0) close(fd_); }
    int fd() const override { return fd_; }
    std::string name() const override { return name_; }

    int read_events(InputEvent* buf, int capacity) override {
        input_event raw[64];
        if (capacity > 64) capacity = 64;
        const ssize_t n = read(fd_, raw, sizeof(input_event) * static_cast<size_t>(capacity));
        if (n < 0) return (errno == EAGAIN || errno == EINTR) ? 0 : -1;  // ENODEV, EIO: the device is gone
        if (n == 0) return -1;
        const int count = static_cast<int>(n / static_cast<ssize_t>(sizeof(input_event)));
        for (int i = 0; i < count; ++i) buf[i] = InputEvent{raw[i].type, raw[i].code, raw[i].value};
        return count;
    }

    bool read_state(std::vector<InputEvent>& out) override {
        out.clear();
        for (uint16_t code : kStateAbs) {
            input_absinfo info{};
            if (ioctl(fd_, EVIOCGABS(code), &info) != 0) return false;
            out.push_back({kEvAbs, code, info.value});
        }
        unsigned char keys[(KEY_MAX + 8) / 8] = {};
        if (ioctl(fd_, EVIOCGKEY(sizeof keys), keys) < 0) return false;
        for (uint16_t code = 0x130; code <= 0x13f; ++code) out.push_back({kEvKey, code, bit_set(keys, code) ? 1 : 0});
        return true;
    }

    bool set_grab(bool on) override { return ioctl(fd_, EVIOCGRAB, on ? 1 : 0) == 0; }

private:
    int fd_;
    std::string name_;
};

class LinuxPad : public PadSink {
public:
    explicit LinuxPad(int fd) : fd_(fd) {}
    ~LinuxPad() override {
        if (fd_ >= 0) { ioctl(fd_, UI_DEV_DESTROY); close(fd_); }
    }
    bool write(const PadReport& report) override {
        InputEvent ev[kMaxPadEvents];
        const size_t n = pad_events(have_prev_ ? &prev_ : nullptr, report, ev);
        input_event raw[kMaxPadEvents];
        std::memset(raw, 0, sizeof raw);
        for (size_t i = 0; i < n; ++i) {
            raw[i].type = ev[i].type;
            raw[i].code = ev[i].code;
            raw[i].value = ev[i].value;
        }
        const ssize_t w = ::write(fd_, raw, sizeof(input_event) * n);
        if (w != static_cast<ssize_t>(sizeof(input_event) * n)) return false;
        prev_ = report;
        have_prev_ = true;
        return true;
    }

private:
    int fd_;
    PadReport prev_;
    bool have_prev_ = false;
};

bool setup_pad(int fd) {
    if (ioctl(fd, UI_SET_EVBIT, EV_KEY) < 0 || ioctl(fd, UI_SET_EVBIT, EV_ABS) < 0) return false;
    for (const PadButton& b : kPadButtons)
        if (ioctl(fd, UI_SET_KEYBIT, b.key_code) < 0) return false;
    struct Axis { uint16_t code; int min, max, fuzz, flat; };
    // Same ranges as the kernel's xpad driver.
    const Axis axes[] = {
        {kAbsX, -32768, 32767, 16, 128}, {kAbsY, -32768, 32767, 16, 128},
        {kAbsRx, -32768, 32767, 16, 128}, {kAbsRy, -32768, 32767, 16, 128},
        {kAbsZ, 0, 255, 0, 0}, {kAbsRz, 0, 255, 0, 0},
        {kAbsHat0X, -1, 1, 0, 0}, {kAbsHat0Y, -1, 1, 0, 0},
    };
    for (const Axis& a : axes) {
        if (ioctl(fd, UI_SET_ABSBIT, a.code) < 0) return false;
        uinput_abs_setup s{};
        s.code = a.code;
        s.absinfo.minimum = a.min;
        s.absinfo.maximum = a.max;
        s.absinfo.fuzz = a.fuzz;
        s.absinfo.flat = a.flat;
        if (ioctl(fd, UI_ABS_SETUP, &s) < 0) return false;
    }
    uinput_setup us{};
    us.id.bustype = BUS_USB;
    us.id.vendor = 0x045e;
    us.id.product = 0x028e;
    us.id.version = 0x0114;
    std::snprintf(us.name, sizeof us.name, "Microsoft X-Box 360 pad");
    if (ioctl(fd, UI_DEV_SETUP, &us) < 0) return false;
    return ioctl(fd, UI_DEV_CREATE) >= 0;
}

}  // namespace

LinuxBackend::LinuxBackend(LinuxBackendPaths paths) : paths_(std::move(paths)) {
    inotify_fd_ = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
    if (inotify_fd_ >= 0) {
        // a new event node, or a permission change on an existing one (the access rule may be applied a moment later)
        if (inotify_add_watch(inotify_fd_, paths_.input_dir.c_str(), IN_CREATE | IN_ATTRIB) < 0) {
            close(inotify_fd_);
            inotify_fd_ = -1;
        }
    }
}

LinuxBackend::~LinuxBackend() {
    if (inotify_fd_ >= 0) close(inotify_fd_);
}

void LinuxBackend::drain_watch() {
    char buf[4096];
    while (read(inotify_fd_, buf, sizeof buf) > 0) {}
}

std::vector<Candidate> LinuxBackend::scan(std::vector<Notice>& problems) {
    std::vector<Candidate> found;
    DIR* dir = opendir(paths_.input_dir.c_str());
    if (!dir) return found;
    while (const dirent* e = readdir(dir)) {
        if (std::strncmp(e->d_name, "event", 5) != 0) continue;
        unsigned vendor = 0;
        if (!read_vendor(paths_.sys_class_input, e->d_name, vendor) || vendor != kVendor8BitDo) continue;
        const std::string path = paths_.input_dir + "/" + e->d_name;
        const int fd = ::open(path.c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
        if (fd < 0) {
            if (errno == EACCES || errno == EPERM) problems.push_back({NoticeKind::NoControllerAccess, path});
            continue;
        }
        if (device_matches(fd)) found.push_back({path});
        close(fd);
    }
    closedir(dir);
    return found;
}

std::unique_ptr<InputSource> LinuxBackend::open(const Candidate& candidate, int* err) {
    const int fd = ::open(candidate.path.c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0) { if (err) *err = errno; return nullptr; }
    char name[256] = {};
    if (ioctl(fd, EVIOCGNAME(sizeof name - 1), name) < 0) std::snprintf(name, sizeof name, "8BitDo controller");
    if (!device_matches(fd)) { close(fd); if (err) *err = ENODEV; return nullptr; }
    return std::make_unique<LinuxSource>(fd, name);
}

std::unique_ptr<PadSink> LinuxBackend::create_pad(int* err) {
    const int fd = ::open(paths_.uinput.c_str(), O_WRONLY | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0) { if (err) *err = errno; return nullptr; }
    if (!setup_pad(fd)) {
        const int e = errno;
        close(fd);
        if (err) *err = e;
        return nullptr;
    }
    return std::make_unique<LinuxPad>(fd);
}

std::unique_ptr<BatteryProvider> LinuxBackend::create_battery_provider() { return make_bluez_battery_provider(); }

}  // namespace u2c::platform
