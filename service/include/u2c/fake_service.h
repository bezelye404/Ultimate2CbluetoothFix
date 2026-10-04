// A service without hardware, for tests and UI demos. It follows the contract of service.h. Call it from one thread.
#pragma once
#include "u2c/service.h"

namespace u2c {

class FakeService : public Service {
public:
    void set_observer(ServiceObserver* observer) override { observer_ = observer; }

    void start() override {
        if (running_) return;
        running_ = true;
        log("LogServicesStarting");
        log("LogMapperStart");
        state_.status = ServiceStatus::Searching;
        if (observer_) observer_->on_status(ServiceStatus::Searching, std::string());
    }

    void stop() override {
        if (!running_) return;
        running_ = false;
        log("LogServicesStopping");
        state_.live_input = PadReport{};
        state_.battery = BatteryInfo{};
        state_.device_name.clear();
        state_.status = ServiceStatus::Stopped;
        state_.live_hz = 0;
        state_.live_ms = 0.0f;
        if (observer_) {
            observer_->on_input(PadReport{});
            observer_->on_battery(BatteryInfo{});
            observer_->on_status(ServiceStatus::Stopped, std::string());
        }
    }

    bool running() const override { return running_; }
    void apply_settings(const Settings& settings) override { settings_ = settings; }
    ServiceState snapshot() const override { return state_; }

    void simulate_connect(const std::string& device_name) {
        if (!running_) return;
        state_.status = ServiceStatus::Connected;
        state_.device_name = device_name;
        if (observer_) {
            observer_->on_log({"LogControllerConnected", {{"name", device_name}}});
            observer_->on_status(ServiceStatus::Connected, device_name);
        }
    }
    void simulate_input(const PadReport& report) {
        if (!running_) return;
        state_.live_input = report;
        if (observer_) observer_->on_input(report);
    }
    void simulate_battery(int level, const std::string& device) {
        if (!running_) return;
        state_.battery = {level, device};
        if (observer_) {
            observer_->on_log({"LogBatteryLevel", {{"name", device}, {"level", std::to_string(level)}}});
            observer_->on_battery(state_.battery);
        }
    }
    void simulate_disconnect() {
        if (!running_ || state_.status != ServiceStatus::Connected) return;
        state_.live_input = PadReport{};
        state_.device_name.clear();
        state_.status = ServiceStatus::Disconnected;
        if (observer_) {
            observer_->on_log({"LogMapperDisconnected", {}});
            observer_->on_input(PadReport{});
            observer_->on_status(ServiceStatus::Disconnected, std::string());
        }
        state_.status = ServiceStatus::Searching;
        if (observer_) observer_->on_status(ServiceStatus::Searching, std::string());
    }
    void simulate_notice(const Notice& notice) {
        if (observer_) observer_->on_notice(notice);
    }

    const Settings& settings() const { return settings_; }

private:
    void log(const char* key) {
        if (observer_) observer_->on_log({key, {}});
    }

    ServiceObserver* observer_ = nullptr;
    bool running_ = false;
    ServiceState state_;
    Settings settings_;
};

}  // namespace u2c
