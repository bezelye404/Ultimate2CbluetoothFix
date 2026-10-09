#pragma once

#include <string>
#include <functional>
#include "Localization.h"
#include <thread>
#include <atomic>

namespace Ultimate2CFixer {

using BatteryCallback = std::function<void(const std::wstring& devName, int level)>;
using LogCallback = std::function<void(const std::wstring&, LogLevel)>;

class BatteryMonitor {
public:
    BatteryMonitor();
    ~BatteryMonitor();

    void Start(LogCallback logCb, BatteryCallback batteryCb);
    void Stop();
    bool IsRunning() const { return m_running.load(); }

private:
    void WorkerLoop();
    bool PollBattery();

    std::atomic<bool> m_running{false};
    std::thread m_workerThread;
    LogCallback m_logCallback;
    BatteryCallback m_batteryCallback;
    std::wstring m_cachedDeviceId;
    int m_lastReportedLevel{-1};
};

} // namespace Ultimate2CFixer
