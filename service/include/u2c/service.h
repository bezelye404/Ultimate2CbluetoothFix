// The controller service as the UI sees it: plain C++ interfaces, no Qt, no OS headers.
// The Linux platform layer has the real implementation; tests and demos use FakeService.
// Contract:
//  - start(): looks for the controller and drives the virtual gamepad. Reports Searching, then Connected
//    (with the device name) once the controller answers.
//  - Controller lost: a zero input report, then Disconnected, then Searching again.
//  - stop(): a zero input report, battery cleared (level -1), status Stopped.
//  - Battery: 0..100, or -1 when unknown.
//  - Threads: the observer is called from the service's own threads. The UI must hand the data to its own thread.
//    Callbacks must not block and must not call back into the service (except snapshot()).
//  - Everything shown as text is a translation key plus values (LogEvent); the UI formats it with its Catalog.
#pragma once
#include <string>
#include <utility>
#include <vector>

#include "u2c/pad.h"
#include "u2c/settings.h"

namespace u2c {

enum class ServiceStatus { Stopped, Searching, Connected, Disconnected };

struct BatteryInfo {
    int level = -1;  // 0..100, -1 = unknown
    std::string device;  // device the level belongs to, empty if unknown
    friend bool operator==(const BatteryInfo&, const BatteryInfo&) = default;
};

// A message for the log box: a translation key and the values of its {placeholders}.
struct LogEvent {
    std::string key;
    std::vector<std::pair<std::string, std::string>> values;
};

// Things the user can fix. The UI shows them as a notice; the texts are in the translation files.
enum class NoticeKind {
    NoControllerAccess,  // the controller's event device cannot be read (permission)
    NoVirtualDevice,  // /dev/uinput cannot be opened for writing (permission or module)
    NoBluetoothService,  // BlueZ not reachable, so no battery information
};
struct Notice {
    NoticeKind kind;
    std::string detail;  // for example the device path, may be empty
};

// Translation keys the service uses in LogEvent. All must exist in every language file (a test checks this).
inline constexpr const char* kNoticeKeys[] = {"NoticeNoControllerAccess", "NoticeNoVirtualDevice", "NoticeNoBluetoothService"};

inline constexpr const char* kServiceLogKeys[] = {
    "LogServicesStarting", "LogServicesStopping", "LogMapperStart", "LogMapperDisconnected",
    "LogControllerConnected", "LogBatteryActive", "LogBatteryLevel", "LogGrabFailed",
};

class ServiceObserver {
public:
    virtual ~ServiceObserver() = default;
    virtual void on_status(ServiceStatus status, const std::string& device_name) = 0;
    virtual void on_battery(const BatteryInfo& battery) = 0;
    virtual void on_input(const PadReport& report) = 0;  // the mapped report
    virtual void on_log(const LogEvent& event) = 0;
    virtual void on_notice(const Notice& notice) = 0;
};

struct ServiceState {
    ServiceStatus status = ServiceStatus::Stopped;
    std::string device_name;  // empty unless Connected
    BatteryInfo battery;
    PadReport live_input;
    int live_hz = 0;  // measured updates in the last second, 0 when not running
    float live_ms = 0.0f;
};

class Service {
public:
    virtual ~Service() = default;
    virtual void set_observer(ServiceObserver* observer) = 0;  // set before start(); nullptr removes it
    virtual void start() = 0;  // no effect if already running
    virtual void stop() = 0;  // no effect if not running
    virtual bool running() const = 0;
    virtual void apply_settings(const Settings& settings) = 0;  // takes effect at once, also while running
    virtual ServiceState snapshot() const = 0;
};

}  // namespace u2c
