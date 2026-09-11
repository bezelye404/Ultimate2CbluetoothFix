#pragma once

#include <string>
#include <functional>
#include <thread>
#include <atomic>

namespace BitDoFixer {

using BatteryCallback = std::function<void(const std::wstring& devName, int level)>;
using LogCallback = std::function<void(const std::wstring&)>;

class BatteryMonitor {
public:
    BatteryMonitor();
    ~BatteryMonitor();

    void Start(LogCallback logCb, BatteryCallback batteryCb);
    void Stop();
    bool IsRunning() const { return m_running.load(); }

private:
    void WorkerLoop();
    void PollBattery();

    std::atomic<bool> m_running{false};
    std::thread m_workerThread;
    LogCallback m_logCallback;
    BatteryCallback m_batteryCallback;
    std::wstring m_cachedDeviceId;
};

} // namespace BitDoFixer
