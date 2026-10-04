#include <poll.h>
#include <sys/eventfd.h>
#include <unistd.h>

#include <atomic>
#include <optional>
#include <chrono>
#include <mutex>
#include <thread>
#include <vector>

#include "../../../core/tests/check.h"
#include "u2c/platform/battery_monitor.h"
#include "u2c/platform/bluez_logic.h"

using namespace u2c;
using namespace u2c::platform;
using Clock = std::chrono::steady_clock;

namespace {

BluezDevice dev(const char* path, const char* name, const char* modalias, bool connected, bool has_battery, int pct) {
    BluezDevice d;
    d.path = path; d.name = name; d.modalias = modalias;
    d.connected = connected; d.has_battery = has_battery; d.percentage = pct;
    return d;
}

int level_of(const std::optional<BatteryInfo>& b) { return b ? b->level : -2; }  // -2: nothing picked

void test_logic() {
    CHECK(is_8bitdo_device(dev("/a", "", "usb:v2DC8p301Bd0001", true, true, 50)));
    CHECK(is_8bitdo_device(dev("/a", "", "usb:v2dc8p301b", true, true, 50)));
    CHECK(is_8bitdo_device(dev("/a", "8BitDo Ultimate 2C Wireless", "", true, true, 50)));
    CHECK(is_8bitdo_device(dev("/a", "my 8BITDO pad", "usb:v1234p0001", true, true, 50)));
    CHECK(!is_8bitdo_device(dev("/a", "Headphones", "usb:v054Cp0CE6", true, true, 50)));
    CHECK(!is_8bitdo_device(dev("/a", "", "", true, true, 50)));

    CHECK(!pick_battery({}).has_value());
    auto pick = pick_battery({dev("/org/bluez/hci0/dev_B", "Headphones", "usb:v054Cp0CE6", true, true, 70),
                              dev("/org/bluez/hci0/dev_A", "8BitDo Ultimate 2C Wireless", "usb:v2DC8p301B", true, true, 87)});
    CHECK_EQ(level_of(pick), 87);
    CHECK(pick && pick->device == "8BitDo Ultimate 2C Wireless");
    // ignored: not connected, no battery interface, out of range, wrong brand
    CHECK(!pick_battery({dev("/a", "8BitDo", "", false, true, 50)}).has_value());
    CHECK(!pick_battery({dev("/a", "8BitDo", "", true, false, 50)}).has_value());
    CHECK(!pick_battery({dev("/a", "8BitDo", "", true, true, 101)}).has_value());
    CHECK(!pick_battery({dev("/a", "8BitDo", "", true, true, -1)}).has_value());
    CHECK(!pick_battery({dev("/a", "Mouse", "usb:v046D", true, true, 50)}).has_value());
    CHECK_EQ(level_of(pick_battery({dev("/a", "8BitDo", "", true, true, 0)})), 0);
    CHECK_EQ(level_of(pick_battery({dev("/a", "8BitDo", "", true, true, 100)})), 100);
    // two controllers: the first by path wins
    pick = pick_battery({dev("/z", "8BitDo Two", "", true, true, 20), dev("/b", "8BitDo One", "", true, true, 30)});
    CHECK(pick && pick->device == "8BitDo One");
    // a nameless device gets a neutral name
    pick = pick_battery({dev("/a", "", "usb:v2DC8p301B", true, true, 40)});
    CHECK(pick && pick->device == "8BitDo controller");
}

// A provider the test drives through a doorbell.
class ScriptedProvider : public BatteryProvider {
public:
    ScriptedProvider() { bell_ = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC); }
    ~ScriptedProvider() override { close(bell_); }
    void start() override { std::lock_guard<std::mutex> l(m_); started_ = true; current_ = next_; available_ = next_available_; }
    int fd() const override { return bell_; }
    short poll_events() const override { return POLLIN; }
    int timeout_ms() const override { return -1; }
    void process() override {
        uint64_t v;
        while (read(bell_, &v, sizeof v) > 0) {}
        std::lock_guard<std::mutex> l(m_);
        current_ = next_;
        available_ = next_available_;
    }
    BatteryInfo current() const override { std::lock_guard<std::mutex> l(m_); return current_; }
    bool available() const override { std::lock_guard<std::mutex> l(m_); return available_; }
    void set(int level, const char* name, bool available = true) {
        { std::lock_guard<std::mutex> l(m_); next_ = {level, name}; next_available_ = available; }
        const uint64_t one = 1;
        const ssize_t w = write(bell_, &one, sizeof one);
        (void)w;
    }
    void preset(int level, const char* name, bool available) { next_ = {level, name}; next_available_ = available; }
private:
    int bell_ = -1;
    mutable std::mutex m_;
    bool started_ = false;
    BatteryInfo current_, next_;
    bool available_ = true, next_available_ = true;
};

template <typename Pred>
bool wait_for(Pred pred, int ms = 2000) {
    const auto end = Clock::now() + std::chrono::milliseconds(ms);
    while (Clock::now() < end) { if (pred()) return true; std::this_thread::sleep_for(std::chrono::milliseconds(2)); }
    return pred();
}

struct Collected {
    std::mutex m;
    std::vector<std::string> events;
    void add(const std::string& e) { std::lock_guard<std::mutex> l(m); events.push_back(e); }
    std::vector<std::string> get() { std::lock_guard<std::mutex> l(m); return events; }
};

BatteryMonitor::Callbacks callbacks_for(Collected& c) {
    BatteryMonitor::Callbacks cb;
    cb.on_started = [&c] { c.add("started"); };
    cb.on_change = [&c](const BatteryInfo& b) { c.add("level:" + std::to_string(b.level)); };
    cb.on_availability = [&c](bool ok) { c.add(ok ? "available" : "unavailable"); };
    return cb;
}

void test_monitor() {
    {  // the first value is reported, changes once, unchanged values are not repeated
        auto provider = std::make_unique<ScriptedProvider>();
        ScriptedProvider* p = provider.get();
        p->preset(80, "pad", true);
        Collected c;
        BatteryMonitor mon(std::move(provider), callbacks_for(c));
        mon.start();
        CHECK(wait_for([&] { return c.get().size() >= 2; }));
        CHECK(c.get() == std::vector<std::string>({"started", "level:80"}));
        p->set(80, "pad");  // woke up, nothing changed
        std::this_thread::sleep_for(std::chrono::milliseconds(40));
        CHECK_EQ(c.get().size(), 2);
        p->set(79, "pad");
        CHECK(wait_for([&] { return c.get().size() >= 3; }));
        p->set(-1, "");  // the controller disconnected
        CHECK(wait_for([&] { return c.get().size() >= 4; }));
        CHECK(c.get() == std::vector<std::string>({"started", "level:80", "level:79", "level:-1"}));
        const auto t0 = Clock::now();
        mon.stop();  // stops at once although the provider has no timeout
        CHECK(Clock::now() - t0 < std::chrono::milliseconds(500));
        mon.stop();  // a second stop is harmless
    }
    {  // the Bluetooth service is missing at the start, comes back, goes away again
        auto provider = std::make_unique<ScriptedProvider>();
        ScriptedProvider* p = provider.get();
        p->preset(-1, "", false);
        Collected c;
        BatteryMonitor mon(std::move(provider), callbacks_for(c));
        mon.start();
        CHECK(wait_for([&] { return c.get().size() >= 2; }));
        CHECK(c.get() == std::vector<std::string>({"started", "unavailable"}));  // no level event while the level is unknown
        p->set(55, "pad", true);
        CHECK(wait_for([&] { return c.get().size() >= 4; }));
        CHECK(c.get() == std::vector<std::string>({"started", "unavailable", "available", "level:55"}));
        p->set(-1, "", false);
        CHECK(wait_for([&] { return c.get().size() >= 6; }));
        CHECK(c.get().back() == "level:-1");
        mon.stop();
    }
    {  // a monitor without a provider does nothing
        BatteryMonitor mon(nullptr, BatteryMonitor::Callbacks{});
        mon.start();
        mon.stop();
        CHECK(true);
    }
    {  // destroyed while running: the destructor stops the thread
        auto provider = std::make_unique<ScriptedProvider>();
        Collected c;
        auto mon = std::make_unique<BatteryMonitor>(std::move(provider), callbacks_for(c));
        mon->start();
        CHECK(wait_for([&] { return !c.get().empty(); }));
        mon.reset();
        CHECK(true);
    }
}

}  // namespace

void test_battery() {
    test_logic();
    test_monitor();
}
