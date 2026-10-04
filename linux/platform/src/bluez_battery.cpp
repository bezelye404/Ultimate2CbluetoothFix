#include "u2c/platform/bluez_battery.h"

#ifdef U2C_HAVE_SDBUS

#include <systemd/sd-bus.h>

#include <cstdint>
#include <cstring>
#include <ctime>
#include <string>
#include <vector>

#include "u2c/platform/bluez_logic.h"

namespace u2c::platform {

namespace {

class BluezBattery : public BatteryProvider {
public:
    ~BluezBattery() override {
        if (bus_) sd_bus_flush_close_unref(bus_);
    }

    void start() override {
        if (sd_bus_open_system(&bus_) < 0) { bus_ = nullptr; available_ = false; return; }
        // Signals that can change what we show: BlueZ appearing or going away, devices added or removed, property changes.
        const char* rules[] = {
            "type='signal',sender='org.freedesktop.DBus',path='/org/freedesktop/DBus',interface='org.freedesktop.DBus',member='NameOwnerChanged',arg0='org.bluez'",
            "type='signal',sender='org.bluez',interface='org.freedesktop.DBus.ObjectManager'",
            "type='signal',sender='org.bluez',interface='org.freedesktop.DBus.Properties',member='PropertiesChanged'",
        };
        for (const char* rule : rules) {
            if (sd_bus_add_match(bus_, nullptr, rule, on_signal, this) < 0) { /* stays usable; we refresh at start only */ }
        }
        refresh();
    }

    int fd() const override { return bus_ ? sd_bus_get_fd(bus_) : -1; }
    short poll_events() const override { return bus_ ? static_cast<short>(sd_bus_get_events(bus_)) : 0; }
    int timeout_ms() const override {
        if (!bus_) return -1;
        uint64_t usec = UINT64_MAX;
        if (sd_bus_get_timeout(bus_, &usec) < 0 || usec == UINT64_MAX) return -1;
        timespec ts{};
        clock_gettime(CLOCK_MONOTONIC, &ts);
        const uint64_t now = static_cast<uint64_t>(ts.tv_sec) * 1000000ULL + static_cast<uint64_t>(ts.tv_nsec) / 1000ULL;
        return usec > now ? static_cast<int>((usec - now) / 1000ULL) + 1 : 0;
    }

    void process() override {
        if (!bus_) return;
        int r;
        while ((r = sd_bus_process(bus_, nullptr)) > 0) {}
        if (r < 0) {
            sd_bus_flush_close_unref(bus_);
            bus_ = nullptr;
            available_ = false;
            current_ = BatteryInfo{};
            return;
        }
        if (dirty_) refresh();
    }

    BatteryInfo current() const override { return current_; }
    bool available() const override { return available_; }

private:
    static int on_signal(sd_bus_message* m, void* userdata, sd_bus_error*) {
        auto* self = static_cast<BluezBattery*>(userdata);
        const char* member = sd_bus_message_get_member(m);
        if (member && std::strcmp(member, "PropertiesChanged") == 0) {
            // only batteries and devices matter, adapters change often
            const char* iface = nullptr;
            if (sd_bus_message_read(m, "s", &iface) < 0 || !iface) return 0;
            if (std::strcmp(iface, "org.bluez.Battery1") != 0 && std::strcmp(iface, "org.bluez.Device1") != 0) return 0;
        }
        self->dirty_ = true;
        return 0;
    }

    static void read_properties(sd_bus_message* m, const char* iface, BluezDevice& dev) {
        if (sd_bus_message_enter_container(m, 'a', "{sv}") < 0) return;
        while (sd_bus_message_enter_container(m, 'e', "sv") > 0) {
            const char* key = nullptr;
            sd_bus_message_read_basic(m, 's', &key);
            const char* contents = nullptr;
            char type = 0;
            sd_bus_message_peek_type(m, &type, &contents);
            bool consumed = false;
            if (type == 'v' && contents && key) {
                const bool device = std::strcmp(iface, "org.bluez.Device1") == 0;
                const bool battery = std::strcmp(iface, "org.bluez.Battery1") == 0;
                if (device && std::strcmp(key, "Name") == 0 && std::strcmp(contents, "s") == 0) {
                    const char* v = nullptr;
                    sd_bus_message_enter_container(m, 'v', "s"); sd_bus_message_read_basic(m, 's', &v); sd_bus_message_exit_container(m);
                    if (v) dev.name = v;
                    consumed = true;
                } else if (device && std::strcmp(key, "Modalias") == 0 && std::strcmp(contents, "s") == 0) {
                    const char* v = nullptr;
                    sd_bus_message_enter_container(m, 'v', "s"); sd_bus_message_read_basic(m, 's', &v); sd_bus_message_exit_container(m);
                    if (v) dev.modalias = v;
                    consumed = true;
                } else if (device && std::strcmp(key, "Connected") == 0 && std::strcmp(contents, "b") == 0) {
                    int v = 0;
                    sd_bus_message_enter_container(m, 'v', "b"); sd_bus_message_read_basic(m, 'b', &v); sd_bus_message_exit_container(m);
                    dev.connected = v != 0;
                    consumed = true;
                } else if (battery && std::strcmp(key, "Percentage") == 0 && std::strcmp(contents, "y") == 0) {
                    uint8_t v = 0;
                    sd_bus_message_enter_container(m, 'v', "y"); sd_bus_message_read_basic(m, 'y', &v); sd_bus_message_exit_container(m);
                    dev.has_battery = true;
                    dev.percentage = v;
                    consumed = true;
                }
            }
            if (!consumed) sd_bus_message_skip(m, "v");
            sd_bus_message_exit_container(m);
        }
        sd_bus_message_exit_container(m);
    }

    // Asks BlueZ for all objects and picks the controller's battery.
    void refresh() {
        dirty_ = false;
        if (!bus_) return;
        sd_bus_error err = SD_BUS_ERROR_NULL;
        sd_bus_message* reply = nullptr;
        const int r = sd_bus_call_method(bus_, "org.bluez", "/", "org.freedesktop.DBus.ObjectManager", "GetManagedObjects", &err, &reply, "");
        if (r < 0) {  // BlueZ is not running or not reachable
            sd_bus_error_free(&err);
            available_ = false;
            current_ = BatteryInfo{};
            return;
        }
        available_ = true;
        std::vector<BluezDevice> devices;
        if (sd_bus_message_enter_container(reply, 'a', "{oa{sa{sv}}}") >= 0) {
            while (sd_bus_message_enter_container(reply, 'e', "oa{sa{sv}}") > 0) {
                const char* path = nullptr;
                sd_bus_message_read_basic(reply, 'o', &path);
                BluezDevice dev;
                if (path) dev.path = path;
                if (sd_bus_message_enter_container(reply, 'a', "{sa{sv}}") >= 0) {
                    while (sd_bus_message_enter_container(reply, 'e', "sa{sv}") > 0) {
                        const char* iface = nullptr;
                        sd_bus_message_read_basic(reply, 's', &iface);
                        read_properties(reply, iface ? iface : "", dev);
                        sd_bus_message_exit_container(reply);
                    }
                    sd_bus_message_exit_container(reply);
                }
                sd_bus_message_exit_container(reply);
                devices.push_back(std::move(dev));
            }
            sd_bus_message_exit_container(reply);
        }
        sd_bus_message_unref(reply);
        const auto pick = pick_battery(std::move(devices));
        current_ = pick ? *pick : BatteryInfo{};
    }

    sd_bus* bus_ = nullptr;
    bool available_ = false;
    bool dirty_ = false;
    BatteryInfo current_;
};

}  // namespace

std::unique_ptr<BatteryProvider> make_bluez_battery_provider() { return std::make_unique<BluezBattery>(); }

}  // namespace u2c::platform

#else  // built without libsystemd

namespace u2c::platform {
std::unique_ptr<BatteryProvider> make_bluez_battery_provider() { return nullptr; }
}  // namespace u2c::platform

#endif
