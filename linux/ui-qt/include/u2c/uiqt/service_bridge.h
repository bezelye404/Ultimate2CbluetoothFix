// Receives service events on the service's threads and hands them to the window in the GUI thread (queued).
#pragma once
#include <QObject>

#include "u2c/service.h"

namespace u2c::uiqt {

class MainWindow;

class ServiceBridge : public QObject, public ServiceObserver {
public:
    explicit ServiceBridge(MainWindow* window) : window_(window) {}
    void on_status(ServiceStatus status, const std::string& device_name) override;
    void on_battery(const BatteryInfo& battery) override;
    void on_input(const PadReport& report) override;
    void on_log(const LogEvent& event) override;
    void on_notice(const Notice& notice) override;

private:
    MainWindow* window_;
};

}  // namespace u2c::uiqt
