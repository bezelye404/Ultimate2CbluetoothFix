#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "../../core/tests/check.h"
#include "u2c/fake_service.h"
#include "u2c/translations.h"

using namespace u2c;

namespace {

struct Recorder : ServiceObserver {
    std::vector<std::string> events;  // short text per event, in order
    ServiceStatus last_status = ServiceStatus::Stopped;
    std::string last_device;
    BatteryInfo last_battery;
    PadReport last_input;
    std::vector<LogEvent> logs;
    std::vector<Notice> notices;

    void on_status(ServiceStatus s, const std::string& d) override {
        last_status = s; last_device = d;
        events.push_back(std::string("status:") + (s == ServiceStatus::Stopped ? "stopped" : s == ServiceStatus::Searching ? "searching" : s == ServiceStatus::Connected ? "connected" : "disconnected"));
    }
    void on_battery(const BatteryInfo& b) override { last_battery = b; events.push_back("battery:" + std::to_string(b.level)); }
    void on_input(const PadReport& r) override { last_input = r; events.push_back(r == PadReport{} ? "input:zero" : "input"); }
    void on_log(const LogEvent& e) override { logs.push_back(e); events.push_back("log:" + e.key); }
    void on_notice(const Notice& n) override { notices.push_back(n); events.push_back("notice"); }
};

// CTest sets U2C_RESOURCE_DIR. Run from the repository folder without it, the default below is used.
std::string resource_dir() {
    const char* dir = std::getenv("U2C_RESOURCE_DIR");
    return dir && dir[0] ? dir : "core/resources";
}

std::string read_file(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    std::stringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

bool same(const std::vector<std::string>& a, const std::vector<std::string>& b) { return a == b; }

void test_start_connect_stop() {
    FakeService svc;
    Recorder rec;
    svc.set_observer(&rec);
    CHECK(!svc.running());
    CHECK(svc.snapshot().status == ServiceStatus::Stopped);

    svc.start();
    CHECK(svc.running());
    CHECK(same(rec.events, {"log:LogServicesStarting", "log:LogMapperStart", "status:searching"}));
    const size_t n = rec.events.size();
    svc.start();
    CHECK_EQ(rec.events.size(), n);

    rec.events.clear();
    svc.simulate_connect("8BitDo Ultimate 2C Wireless");
    CHECK(same(rec.events, {"log:LogControllerConnected", "status:connected"}));
    CHECK(rec.logs.back().values.size() == 1 && rec.logs.back().values[0].second == "8BitDo Ultimate 2C Wireless");
    CHECK(svc.snapshot().status == ServiceStatus::Connected);
    CHECK(svc.snapshot().device_name == "8BitDo Ultimate 2C Wireless");

    PadReport r;
    r.buttons = kA; r.lx = 1234;
    svc.simulate_input(r);
    CHECK(rec.last_input == r);
    CHECK(svc.snapshot().live_input == r);

    svc.simulate_battery(80, "8BitDo Ultimate 2C Wireless");
    CHECK_EQ(rec.last_battery.level, 80);
    CHECK_EQ(svc.snapshot().battery.level, 80);

    rec.events.clear();
    svc.stop();  // zero input, battery cleared, Stopped
    CHECK(same(rec.events, {"log:LogServicesStopping", "input:zero", "battery:-1", "status:stopped"}));
    CHECK(!svc.running());
    const ServiceState s = svc.snapshot();
    CHECK(s.status == ServiceStatus::Stopped);
    CHECK(s.device_name.empty());
    CHECK_EQ(s.battery.level, -1);
    CHECK(s.live_input == PadReport{});

    rec.events.clear();
    svc.stop();
    CHECK(rec.events.empty());
    svc.simulate_input(r);
    CHECK(rec.events.empty());
}

void test_disconnect_sequence() {
    FakeService svc;
    Recorder rec;
    svc.set_observer(&rec);
    svc.start();
    svc.simulate_connect("Pad");
    PadReport r; r.buttons = kB;
    svc.simulate_input(r);
    rec.events.clear();
    svc.simulate_disconnect();
    // zero input, Disconnected, then Searching again
    CHECK(same(rec.events, {"log:LogMapperDisconnected", "input:zero", "status:disconnected", "status:searching"}));
    CHECK(svc.snapshot().status == ServiceStatus::Searching);
    CHECK(svc.snapshot().device_name.empty());
    rec.events.clear();
    svc.simulate_disconnect();  // not connected: nothing happens
    CHECK(rec.events.empty());
}

void test_settings_and_notices() {
    FakeService svc;
    Settings s;
    s.nintendo_mode = true; s.deadzone = 3;
    svc.apply_settings(s);  // also allowed while stopped
    CHECK(svc.settings() == s);
    CHECK_EQ(to_map_config(svc.settings()).deadzone, 6500);

    Recorder rec;
    svc.set_observer(&rec);
    svc.simulate_notice({NoticeKind::NoVirtualDevice, "/dev/uinput"});
    CHECK_EQ(rec.notices.size(), 1);
    CHECK(rec.notices[0].kind == NoticeKind::NoVirtualDevice);
    CHECK(rec.notices[0].detail == "/dev/uinput");

    svc.set_observer(nullptr);  // no observer: nothing may crash
    svc.start();
    svc.simulate_connect("x");
    svc.simulate_input(PadReport{});
    svc.simulate_battery(1, "x");
    svc.simulate_disconnect();
    svc.simulate_notice({NoticeKind::NoBluetoothService, ""});
    svc.stop();
    CHECK(true);
}

// Every translation key the service uses must exist in every language file.
void test_log_keys_exist() {
    for (const char* lang : {"en", "tr"}) {
        const Translations t = Translations::parse(read_file(resource_dir() + "/lang/" + lang + ".txt"));
        CHECK(t.size() > 0);
        for (const char* key : kServiceLogKeys) {
            CHECK(t.has(key));
            if (!t.has(key)) std::printf("  missing key %s in %s\n", key, lang);
        }
        for (const char* key : kNoticeKeys) {
            CHECK(t.has(key));
            if (!t.has(key)) std::printf("  missing key %s in %s\n", key, lang);
        }
    }
}

}  // namespace

int main() {
    test_start_connect_stop();
    test_disconnect_sequence();
    test_settings_and_notices();
    test_log_keys_exist();
    std::printf("checks: %ld, failures: %ld\n", u2ctest::g_checks, u2ctest::g_failures);
    return u2ctest::g_failures == 0 ? 0 : 1;
}

void test_settings() {}
void test_translations() {}
void test_rate_limiter() {}
