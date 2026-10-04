// Tests of LinuxService with fake devices: the real service thread, epoll and timers run, only the controller and
// the virtual pad are fakes. No hardware, no system change.
#include <dirent.h>
#include <poll.h>
#include <fcntl.h>
#include <sys/eventfd.h>
#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <deque>
#include <mutex>
#include <thread>

#include "../../../core/tests/check.h"
#include "u2c/axes.h"
#include "u2c/buttons.h"
#include "u2c/platform/linux_service.h"

using namespace u2c;
using namespace u2c::platform;
using Clock = std::chrono::steady_clock;

namespace {

// What the fakes share with the test, so the objects owned by the service can be watched safely.
struct World {
    std::mutex m;
    bool device_present = false;
    bool device_lost = false;
    std::string device_name = "8BitDo Ultimate 2C Wireless";
    std::deque<InputEvent> queue;
    std::vector<InputEvent> state;                 // answer of read_state()
    int doorbell[2] = {-1, -1};
    int open_error = 0;
    bool grab_ok = true;
    std::vector<bool> grab_calls;
    int sources_alive = 0, sources_created = 0;
    std::vector<Notice> scan_problems;
    int pad_error = 0;
    int pads_alive = 0, pads_created = 0;
    struct Write { PadReport report; Clock::time_point at; };
    std::vector<Write> writes;
    bool battery_enabled = false;
    int battery_bell = -1;
    BatteryInfo battery_next;
    bool battery_next_available = true;
    int watch_fd = -1;
    int scans = 0;
    // time of the last pad destruction check
    World() {
        if (pipe2(doorbell, O_NONBLOCK | O_CLOEXEC) != 0) doorbell[0] = doorbell[1] = -1;
        watch_fd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
        battery_bell = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
        set_rest_state();
    }
    ~World() { close(doorbell[0]); close(doorbell[1]); close(watch_fd); close(battery_bell); }
    void set_battery(int level, const std::string& name, bool available = true) {
        { std::lock_guard<std::mutex> l(m); battery_next = {level, name}; battery_next_available = available; }
        const uint64_t one = 1; const ssize_t w = write(battery_bell, &one, sizeof one); (void)w;
    }

    void set_rest_state() {
        state = {{3, 0x00, 127}, {3, 0x01, 127}, {3, 0x02, 127}, {3, 0x05, 127}, {3, 0x09, 0}, {3, 0x0a, 0}, {3, 0x10, 0}, {3, 0x11, 0}};
        for (uint16_t code = 0x130; code <= 0x13f; ++code) state.push_back({1, code, 0});
    }
    void poke_watch() { const uint64_t one = 1; const ssize_t w = write(watch_fd, &one, sizeof one); (void)w; }
    void ring() { const char c = 1; const ssize_t w = write(doorbell[1], &c, 1); (void)w; }
    void push(std::initializer_list<InputEvent> list) {
        { std::lock_guard<std::mutex> l(m); for (const auto& e : list) queue.push_back(e); }
        ring();
    }
    void set_state(uint16_t type, uint16_t code, int value) {
        std::lock_guard<std::mutex> l(m);
        for (auto& e : state) if (e.type == type && e.code == code) e.value = value;
    }
    void set_pad_error(int e) { std::lock_guard<std::mutex> l(m); pad_error = e; }
    void set_open_error(int e) { std::lock_guard<std::mutex> l(m); open_error = e; }
    void clear_scan_problems() { std::lock_guard<std::mutex> l(m); scan_problems.clear(); }
    int pads_alive_now() { std::lock_guard<std::mutex> l(m); return pads_alive; }
    int sources_alive_now() { std::lock_guard<std::mutex> l(m); return sources_alive; }
    int pads_created_now() { std::lock_guard<std::mutex> l(m); return pads_created; }
    void lose_device() { { std::lock_guard<std::mutex> l(m); device_lost = true; device_present = false; } ring(); }  // the device node disappears
    void add_device() { { std::lock_guard<std::mutex> l(m); device_present = true; device_lost = false; queue.clear(); } poke_watch(); }
    size_t write_count() { std::lock_guard<std::mutex> l(m); return writes.size(); }
    PadReport last_write() { std::lock_guard<std::mutex> l(m); return writes.empty() ? PadReport{} : writes.back().report; }
};

class FakeSource : public InputSource {
public:
    explicit FakeSource(World* w) : w_(w) { std::lock_guard<std::mutex> l(w_->m); ++w_->sources_alive; ++w_->sources_created; }
    ~FakeSource() override { std::lock_guard<std::mutex> l(w_->m); --w_->sources_alive; }
    int fd() const override { return w_->doorbell[0]; }
    std::string name() const override { return w_->device_name; }
    int read_events(InputEvent* buf, int capacity) override {
        char junk[64];
        while (read(w_->doorbell[0], junk, sizeof junk) > 0) {}
        std::lock_guard<std::mutex> l(w_->m);
        if (w_->queue.empty()) return w_->device_lost ? -1 : 0;
        int n = 0;
        while (n < capacity && !w_->queue.empty()) { buf[n++] = w_->queue.front(); w_->queue.pop_front(); }
        if (!w_->queue.empty()) w_->ring();  // more waiting: keep the doorbell ringing (level triggered)
        return n;
    }
    bool read_state(std::vector<InputEvent>& out) override { std::lock_guard<std::mutex> l(w_->m); out = w_->state; return true; }
    bool set_grab(bool on) override { std::lock_guard<std::mutex> l(w_->m); w_->grab_calls.push_back(on); return w_->grab_ok; }
private:
    World* w_;
};

class FakePad : public PadSink {
public:
    explicit FakePad(World* w) : w_(w) { std::lock_guard<std::mutex> l(w_->m); ++w_->pads_alive; ++w_->pads_created; }
    ~FakePad() override { std::lock_guard<std::mutex> l(w_->m); --w_->pads_alive; }
    bool write(const PadReport& r) override { std::lock_guard<std::mutex> l(w_->m); w_->writes.push_back({r, Clock::now()}); return true; }
private:
    World* w_;
};

class FakeBattery : public BatteryProvider {
public:
    explicit FakeBattery(World* w) : w_(w) {}
    void start() override { process(); }
    int fd() const override { return w_->battery_bell; }
    short poll_events() const override { return POLLIN; }
    int timeout_ms() const override { return -1; }
    void process() override {
        uint64_t v;
        while (read(w_->battery_bell, &v, sizeof v) > 0) {}
        std::lock_guard<std::mutex> l(w_->m);
        current_ = w_->battery_next;
        available_ = w_->battery_next_available;
    }
    BatteryInfo current() const override { return current_; }
    bool available() const override { return available_; }
private:
    World* w_;
    BatteryInfo current_;
    bool available_ = true;
};

class FakeBackend : public Backend {
public:
    explicit FakeBackend(std::shared_ptr<World> w) : w_(std::move(w)) {}
    int watch_fd() override { return w_->watch_fd; }
    void drain_watch() override { uint64_t v; while (read(w_->watch_fd, &v, sizeof v) > 0) {} }
    std::vector<Candidate> scan(std::vector<Notice>& problems) override {
        std::lock_guard<std::mutex> l(w_->m);
        ++w_->scans;
        for (const auto& n : w_->scan_problems) problems.push_back(n);
        std::vector<Candidate> c;
        if (w_->device_present) c.push_back({"/dev/input/event-fake"});
        return c;
    }
    std::unique_ptr<InputSource> open(const Candidate&, int* err) override {
        { std::lock_guard<std::mutex> l(w_->m); if (w_->open_error) { *err = w_->open_error; return nullptr; } }
        return std::make_unique<FakeSource>(w_.get());
    }
    std::unique_ptr<BatteryProvider> create_battery_provider() override {
        { std::lock_guard<std::mutex> l(w_->m); if (!w_->battery_enabled) return nullptr; }
        return std::make_unique<FakeBattery>(w_.get());
    }
    std::unique_ptr<PadSink> create_pad(int* err) override {
        { std::lock_guard<std::mutex> l(w_->m); if (w_->pad_error) { *err = w_->pad_error; return nullptr; } }
        return std::make_unique<FakePad>(w_.get());
    }
private:
    std::shared_ptr<World> w_;
};

struct Recorder : ServiceObserver {
    std::mutex m;
    std::vector<std::string> events;
    std::vector<Notice> notices;
    std::vector<PadReport> inputs;
    size_t notice_count() { std::lock_guard<std::mutex> l(m); return notices.size(); }
    Notice notice(size_t i) { std::lock_guard<std::mutex> l(m); return notices.at(i); }
    int count(const std::string& e) { std::lock_guard<std::mutex> l(m); return static_cast<int>(std::count(events.begin(), events.end(), e)); }
    std::vector<std::string> copy() { std::lock_guard<std::mutex> l(m); return events; }
    void clear() { std::lock_guard<std::mutex> l(m); events.clear(); notices.clear(); inputs.clear(); }
    void on_status(ServiceStatus s, const std::string&) override {
        std::lock_guard<std::mutex> l(m);
        events.push_back(s == ServiceStatus::Stopped ? "stopped" : s == ServiceStatus::Searching ? "searching" : s == ServiceStatus::Connected ? "connected" : "disconnected");
    }
    void on_battery(const BatteryInfo& b) override { std::lock_guard<std::mutex> l(m); events.push_back("battery:" + std::to_string(b.level)); }
    void on_input(const PadReport& r) override { std::lock_guard<std::mutex> l(m); inputs.push_back(r); events.push_back(r == PadReport{} ? "input:zero" : "input"); }
    void on_log(const LogEvent& e) override { std::lock_guard<std::mutex> l(m); events.push_back("log:" + e.key); }
    void on_notice(const Notice& n) override { std::lock_guard<std::mutex> l(m); notices.push_back(n); events.push_back("notice"); }
};

template <typename Pred>
bool wait_for(Pred pred, int timeout_ms = 3000) {
    const auto end = Clock::now() + std::chrono::milliseconds(timeout_ms);
    while (Clock::now() < end) {
        if (pred()) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    return pred();
}
void sleep_ms(int ms) { std::this_thread::sleep_for(std::chrono::milliseconds(ms)); }

struct Rig {
    std::shared_ptr<World> world = std::make_shared<World>();
    Recorder rec;
    std::unique_ptr<LinuxService> svc;
    explicit Rig(int retry_ms = 5000) {
        svc = std::make_unique<LinuxService>(std::make_unique<FakeBackend>(world), LinuxServiceOptions{retry_ms});
        svc->set_observer(&rec);
    }
    bool connected() { return rec.count("connected") >= 1 && svc->snapshot().status == ServiceStatus::Connected; }
    void connect() { svc->start(); world->add_device(); wait_for([&] { return connected(); }); }
};

int open_fd_count() {
    int n = 0;
    DIR* d = opendir("/proc/self/fd");
    while (d && readdir(d)) ++n;
    if (d) closedir(d);
    return n;
}

void test_connect_and_input() {
    Rig r;
    r.svc->start();
    CHECK(wait_for([&] { return r.rec.count("searching") >= 1; }));
    CHECK(r.svc->running());
    CHECK(r.svc->snapshot().status == ServiceStatus::Searching);
    CHECK_EQ(r.world->write_count(), 0);

    r.world->add_device();
    CHECK(wait_for([&] { return r.connected(); }));
    CHECK(r.svc->snapshot().device_name == "8BitDo Ultimate 2C Wireless");
    CHECK(wait_for([&] { return r.world->write_count() >= 1; }));  // the first state goes to the virtual pad at once
    CHECK(r.world->last_write() == PadReport{});  // sticks at rest give an all-zero report
    CHECK_EQ(r.world->pads_alive_now(), 1);
    CHECK(r.rec.copy().size() >= 5);
    {
        const auto ev = r.rec.copy();
        CHECK(ev[0] == "log:LogServicesStarting" && ev[1] == "log:LogMapperStart" && ev[2] == "searching");
        const auto it = std::find(ev.begin(), ev.end(), "log:LogControllerConnected");
        CHECK(it != ev.end() && *(it + 1) == "connected");
    }

    r.world->push({{3, 0x00, 255}, {1, 0x130, 1}, {0, 0, 0}});  // left stick right, A pressed
    CHECK(wait_for([&] { return r.world->last_write().lx == 32767; }));
    CHECK_EQ(r.world->last_write().buttons, kA);
    CHECK(r.svc->snapshot().live_input.lx == 32767);

    r.world->push({{3, 0x0a, 100}, {1, 0x138, 1}, {0, 0, 0}});  // left trigger analog value and its click
    CHECK(wait_for([&] { return r.world->last_write().left_trigger == 100; }));  // the click does not force 255
    r.svc->stop();
    CHECK(!r.svc->running());
}

void test_rate_limit() {
    Rig r;
    Settings s;
    s.polling_rate = 0;  // 125 Hz = 8 ms
    r.svc->apply_settings(s);
    r.connect();
    CHECK(wait_for([&] { return r.world->write_count() >= 1; }));
    const size_t before = r.world->write_count();
    const auto t0 = Clock::now();
    int value = 0;
    while (Clock::now() - t0 < std::chrono::milliseconds(200)) {  // a burst: a change every 0.1 ms for 200 ms
        value = (value + 37) % 256;
        r.world->push({{3, 0x00, value}, {0, 0, 0}});
        std::this_thread::sleep_for(std::chrono::microseconds(100));
    }
    const int last_value = value;
    CHECK(wait_for([&] { return r.world->last_write().lx == stick_axis(scale_8_to_16(last_value), 4000, 0); }));
    sleep_ms(40);
    std::vector<World::Write> w;
    { std::lock_guard<std::mutex> l(r.world->m); w.assign(r.world->writes.begin() + static_cast<long>(before), r.world->writes.end()); }
    const auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - t0).count();
    CHECK(w.size() >= 10);
    CHECK(static_cast<long>(w.size()) <= elapsed_ms / 8 + 3);  // never faster than 125 Hz
    for (size_t i = 1; i < w.size(); ++i) {
        const auto gap = std::chrono::duration_cast<std::chrono::microseconds>(w[i].at - w[i - 1].at).count();
        CHECK(gap >= 7000);  // 8 ms, with 1 ms tolerance for scheduling
    }
    r.svc->stop();
}

void test_disconnect_and_reconnect() {
    Rig r;
    r.connect();
    r.world->push({{1, 0x130, 1}, {0, 0, 0}});
    CHECK(wait_for([&] { return r.world->last_write().buttons == kA; }));
    r.rec.clear();
    r.world->lose_device();
    CHECK(wait_for([&] { return r.rec.count("searching") >= 1; }));
    CHECK(r.rec.copy() == std::vector<std::string>({"log:LogMapperDisconnected", "input:zero", "disconnected", "searching"}));
    CHECK_EQ(r.world->last_write().buttons, 0);  // buttons were released before the pad was removed
    CHECK(wait_for([&] { return r.world->pads_alive_now() == 0 && r.world->sources_alive_now() == 0; }));
    CHECK(r.svc->snapshot().device_name.empty());

    r.rec.clear();
    r.world->add_device();  // the controller comes back
    CHECK(wait_for([&] { return r.connected(); }));
    CHECK_EQ(r.world->pads_created_now(), 2);
    r.svc->stop();
}

void test_stop_order_and_restart() {
    Rig r;
    r.connect();
    r.world->push({{1, 0x131, 1}, {0, 0, 0}});
    CHECK(wait_for([&] { return r.world->last_write().buttons == kB; }));
    r.rec.clear();
    r.svc->stop();
    CHECK(r.rec.copy() == std::vector<std::string>({"log:LogServicesStopping", "input:zero", "battery:-1", "stopped"}));
    CHECK_EQ(r.world->last_write().buttons, 0);
    CHECK_EQ(r.world->pads_alive_now(), 0);
    CHECK_EQ(r.world->sources_alive_now(), 0);
    CHECK(r.svc->snapshot().status == ServiceStatus::Stopped);
    r.svc->stop();
    CHECK_EQ(r.rec.count("stopped"), 1);

    r.world->add_device();  // start again after a stop
    r.svc->start();
    CHECK(wait_for([&] { return r.connected(); }));
    CHECK_EQ(r.world->pads_created_now(), 2);
    r.svc->stop();
}

void test_notices() {
    Rig r;
    r.world->scan_problems.push_back({NoticeKind::NoControllerAccess, "/dev/input/event9"});
    r.svc->start();
    CHECK(wait_for([&] { return r.rec.notice_count() >= 1; }));
    for (int i = 0; i < 5; ++i) { r.world->poke_watch(); sleep_ms(10); }  // more scans must not repeat the notice
    sleep_ms(30);
    CHECK_EQ(r.rec.notice_count(), 1);
    CHECK(r.rec.notice(0).kind == NoticeKind::NoControllerAccess);
    CHECK(r.rec.notice(0).detail == "/dev/input/event9");
    r.world->clear_scan_problems();

    r.world->set_open_error(EACCES);  // the device is there, but cannot be opened
    r.world->add_device();
    CHECK(wait_for([&] { return r.rec.notice_count() >= 2; }));
    CHECK(r.rec.notice(1).kind == NoticeKind::NoControllerAccess);
    CHECK(r.svc->snapshot().status == ServiceStatus::Searching);
    r.svc->stop();
}

void test_no_virtual_device_then_retry() {
    Rig r(60);  // retry after 60 ms
    r.world->set_pad_error(EACCES);
    r.svc->start();
    r.world->add_device();
    CHECK(wait_for([&] { return r.rec.notice_count() >= 1; }));
    CHECK(r.rec.notice(0).kind == NoticeKind::NoVirtualDevice);
    sleep_ms(200);  // several retries fail: still one notice, still searching
    CHECK_EQ(r.rec.notice_count(), 1);
    CHECK(r.svc->snapshot().status == ServiceStatus::Searching);
    CHECK_EQ(r.world->sources_alive_now(), 0);  // the device is not kept open without a pad
    CHECK_EQ(r.rec.count("connected"), 0);
    r.world->set_pad_error(0);  // the problem is fixed
    CHECK(wait_for([&] { return r.connected(); }));
    r.svc->stop();
}

void test_settings_live() {
    Rig r;
    r.connect();
    r.world->push({{1, 0x130, 1}, {0, 0, 0}});
    CHECK(wait_for([&] { return r.world->last_write().buttons == kA; }));
    Settings s;
    s.nintendo_mode = true;
    r.svc->apply_settings(s);  // A and B swap while the button is held
    CHECK(wait_for([&] { return r.world->last_write().buttons == kB; }));
    s.nintendo_mode = false;
    s.hair_trigger = true;
    r.svc->apply_settings(s);
    CHECK(wait_for([&] { return r.world->last_write().buttons == kA; }));
    r.world->push({{3, 0x09, 10}, {0, 0, 0}});  // 10 is above the trigger threshold: hair trigger gives 255
    CHECK(wait_for([&] { return r.world->last_write().right_trigger == 255; }));
    r.svc->stop();

    // settings handed over before start() are used from the first write
    Rig r2;
    Settings n;
    n.nintendo_mode = true;
    r2.svc->apply_settings(n);
    r2.world->set_state(1, 0x130, 1);
    r2.connect();
    CHECK(wait_for([&] { return r2.world->write_count() >= 1; }));
    CHECK_EQ(r2.world->last_write().buttons, kB);
    r2.svc->stop();
}

void test_grab() {
    Rig r;
    r.connect();
    CHECK(wait_for([&] { std::lock_guard<std::mutex> l(r.world->m); return !r.world->grab_calls.empty(); }));
    { std::lock_guard<std::mutex> l(r.world->m); CHECK(r.world->grab_calls.back() == true); }
    Settings s;
    s.exclusive_grab = false;
    r.svc->apply_settings(s);
    CHECK(wait_for([&] { std::lock_guard<std::mutex> l(r.world->m); return r.world->grab_calls.back() == false; }));
    s.exclusive_grab = true;
    r.svc->apply_settings(s);
    CHECK(wait_for([&] { std::lock_guard<std::mutex> l(r.world->m); return r.world->grab_calls.back() == true; }));
    r.svc->stop();

    Rig failing;  // the grab cannot be taken (for example another program holds it)
    failing.world->grab_ok = false;
    failing.connect();
    CHECK(failing.connected());  // the service still works
    CHECK(wait_for([&] { return failing.rec.count("log:LogGrabFailed") >= 1; }));
    failing.svc->stop();

    Rig off;  // grab switched off before the start: never requested
    Settings no;
    no.exclusive_grab = false;
    off.svc->apply_settings(no);
    off.connect();
    sleep_ms(30);
    { std::lock_guard<std::mutex> l(off.world->m); CHECK(off.world->grab_calls.empty()); }
    off.svc->stop();
}

void test_battery() {
    Rig r;
    r.world->battery_enabled = true;
    r.world->battery_next = {80, "8BitDo Ultimate 2C Wireless"};
    r.svc->start();
    CHECK(wait_for([&] { return r.rec.count("battery:80") >= 1; }));
    {
        const auto ev = r.rec.copy();  // battery monitor active, then the level
        const auto a = std::find(ev.begin(), ev.end(), "log:LogBatteryActive");
        const auto b = std::find(ev.begin(), ev.end(), "log:LogBatteryLevel");
        CHECK(a != ev.end() && b != ev.end() && a < b);
    }
    CHECK_EQ(r.svc->snapshot().battery.level, 80);  // also without a controller connected
    CHECK(r.svc->snapshot().battery.device == "8BitDo Ultimate 2C Wireless");

    r.world->set_battery(80, "8BitDo Ultimate 2C Wireless");  // woke up, nothing changed: no new events
    sleep_ms(40);
    CHECK_EQ(r.rec.count("battery:80"), 1);
    CHECK_EQ(r.rec.count("log:LogBatteryLevel"), 1);
    r.world->set_battery(79, "8BitDo Ultimate 2C Wireless");
    CHECK(wait_for([&] { return r.rec.count("battery:79") >= 1; }));
    CHECK_EQ(r.rec.count("log:LogBatteryLevel"), 2);
    CHECK_EQ(r.svc->snapshot().battery.level, 79);

    r.world->set_battery(-1, "");  // the controller went away: level unknown, no log line
    CHECK(wait_for([&] { return r.rec.count("battery:-1") >= 1; }));
    CHECK_EQ(r.rec.count("log:LogBatteryLevel"), 2);
    CHECK_EQ(r.svc->snapshot().battery.level, -1);
    CHECK_EQ(r.rec.notice_count(), 0);

    r.world->set_battery(-1, "", false);  // the Bluetooth service disappears: one notice
    CHECK(wait_for([&] { return r.rec.notice_count() >= 1; }));
    CHECK(r.rec.notice(0).kind == NoticeKind::NoBluetoothService);
    r.world->set_battery(-1, "", false);
    sleep_ms(40);
    CHECK_EQ(r.rec.notice_count(), 1);  // not repeated while it stays away
    r.world->set_battery(60, "pad", true);
    CHECK(wait_for([&] { return r.rec.count("battery:60") >= 1; }));
    r.world->set_battery(-1, "", false);  // gone again: told again
    CHECK(wait_for([&] { return r.rec.notice_count() >= 2; }));

    r.world->set_battery(42, "pad", true);
    CHECK(wait_for([&] { return r.rec.count("battery:42") >= 1; }));
    r.rec.clear();
    const auto t0 = Clock::now();
    r.svc->stop();  // the battery thread stops with the service, promptly
    CHECK(Clock::now() - t0 < std::chrono::milliseconds(500));
    CHECK(r.rec.copy() == std::vector<std::string>({"log:LogServicesStopping", "input:zero", "battery:-1", "stopped"}));
    CHECK_EQ(r.svc->snapshot().battery.level, -1);
    r.world->set_battery(90, "pad", true);  // after the stop nothing is reported any more
    sleep_ms(40);
    CHECK_EQ(r.rec.count("battery:90"), 0);
}

void test_no_battery_source() {
    Rig r;  // a backend without battery support
    r.connect();
    sleep_ms(40);
    CHECK_EQ(r.rec.count("log:LogBatteryActive"), 0);
    CHECK_EQ(r.svc->snapshot().battery.level, -1);
    r.svc->stop();
}

void test_unchanged_output_is_not_written() {
    Rig r;
    r.connect();
    CHECK(wait_for([&] { return r.world->write_count() >= 1; }));
    sleep_ms(30);
    const size_t before = r.world->write_count();
    for (int v : {128, 126, 129, 127, 130, 125})  // movements inside the dead zone: the report stays all zero
        r.world->push({{3, 0x00, v}, {0, 0, 0}});
    r.world->push({{0, 0, 0}});
    sleep_ms(80);
    CHECK_EQ(r.world->write_count(), before);  // nothing was written, no duplicates
    r.world->push({{3, 0x00, 255}, {0, 0, 0}});
    CHECK(wait_for([&] { return r.world->write_count() == before + 1; }));
    sleep_ms(40);
    CHECK_EQ(r.world->write_count(), before + 1);
    r.svc->stop();
}

void test_dropped_events() {
    Rig r;
    r.connect();
    CHECK(wait_for([&] { return r.world->write_count() >= 1; }));
    r.world->set_state(3, 0x00, 255);  // while events were dropped, the stick moved
    r.world->push({{0, 3, 0}});
    CHECK(wait_for([&] { return r.world->last_write().lx == 32767; }));
    r.svc->stop();
}

void test_idle_cost_and_resources() {
    Rig r;
    r.connect();
    CHECK(wait_for([&] { return r.world->write_count() >= 1; }));
    sleep_ms(50);
    timespec a{}, b{};
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &a);
    sleep_ms(600);  // connected, controller idle
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &b);
    const double cpu_ms = (b.tv_sec - a.tv_sec) * 1000.0 + (b.tv_nsec - a.tv_nsec) / 1e6;
    CHECK(cpu_ms < 20.0);  // nothing runs while the controller is idle
    r.svc->stop();

    const int fds_before = open_fd_count();
    for (int i = 0; i < 30; ++i) {
        Rig c;
        c.connect();
        c.world->lose_device();
        wait_for([&] { return c.rec.count("searching") >= 2; });
        c.svc->stop();
        CHECK(c.world->pads_alive_now() == 0 && c.world->sources_alive_now() == 0);
    }
    CHECK_EQ(open_fd_count(), fds_before);  // no file descriptor leaks over 30 connect and stop cycles
}

void test_snapshot_rate() {
    Rig r;
    r.connect();
    CHECK(wait_for([&] { return r.world->write_count() >= 1; }));
    (void)r.svc->snapshot();  // starts the measuring window
    const auto t0 = Clock::now();
    int v = 0;
    while (Clock::now() - t0 < std::chrono::milliseconds(300)) {  // about 30 changes in 0.3 s
        v = (v + 50) % 256;
        r.world->push({{3, 0x01, v}, {0, 0, 0}});
        sleep_ms(10);
    }
    sleep_ms(static_cast<int>(1050 - std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - t0).count()));
    const ServiceState s = r.svc->snapshot();
    CHECK(s.live_hz >= 10 && s.live_hz <= 60);
    CHECK(s.live_ms > 10.0f && s.live_ms < 100.0f);
    sleep_ms(1100);  // idle for a full second: 0 Hz
    CHECK_EQ(r.svc->snapshot().live_hz, 0);
    r.svc->stop();
    CHECK_EQ(r.svc->snapshot().live_hz, 0);
}

}  // namespace

void test_linux_service() {
    test_connect_and_input();
    test_rate_limit();
    test_disconnect_and_reconnect();
    test_stop_order_and_restart();
    test_notices();
    test_no_virtual_device_then_retry();
    test_settings_live();
    test_grab();
    test_battery();
    test_no_battery_source();
    test_unchanged_output_is_not_written();
    test_dropped_events();
    test_idle_cost_and_resources();
    test_snapshot_rate();
}
