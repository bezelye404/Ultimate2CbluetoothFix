#include "u2c/uiqt/service_bridge.h"

#include <QMetaObject>

#include "u2c/uiqt/main_window.h"

namespace u2c::uiqt {

// Every call copies its data and runs in the window's thread later, so the service thread never touches widgets.
void ServiceBridge::on_status(ServiceStatus status, const std::string& device_name) {
    MainWindow* w = window_;
    QMetaObject::invokeMethod(w, [w, status, device_name] { w->handle_status(status, device_name); }, Qt::QueuedConnection);
}

void ServiceBridge::on_battery(const BatteryInfo& battery) {
    MainWindow* w = window_;
    QMetaObject::invokeMethod(w, [w, battery] { w->handle_battery(battery); }, Qt::QueuedConnection);
}

void ServiceBridge::on_input(const PadReport& report) {
    MainWindow* w = window_;
    QMetaObject::invokeMethod(w, [w, report] { w->handle_input(report); }, Qt::QueuedConnection);
}

void ServiceBridge::on_log(const LogEvent& event) {
    MainWindow* w = window_;
    QMetaObject::invokeMethod(w, [w, event] { w->handle_log(event); }, Qt::QueuedConnection);
}

void ServiceBridge::on_notice(const Notice& notice) {
    MainWindow* w = window_;
    QMetaObject::invokeMethod(w, [w, notice] { w->handle_notice(notice); }, Qt::QueuedConnection);
}

}  // namespace u2c::uiqt
