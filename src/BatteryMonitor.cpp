#include "BatteryMonitor.h"
#include "Localization.h"
#include <windows.h>
#include <cwctype>
#include <algorithm>

#if __has_include(<winrt/Windows.Devices.Bluetooth.GenericAttributeProfile.h>)
#define HAS_WINRT_BLE 1
#include <winrt/base.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Devices.Bluetooth.h>
#include <winrt/Windows.Devices.Bluetooth.GenericAttributeProfile.h>
#include <winrt/Windows.Devices.Enumeration.h>
#include <winrt/Windows.Storage.Streams.h>
#pragma comment(lib, "windowsapp.lib")
#else
#define HAS_WINRT_BLE 0
#endif

namespace Ultimate2CFixer {

BatteryMonitor::BatteryMonitor() = default;

BatteryMonitor::~BatteryMonitor() {
    Stop();
}

void BatteryMonitor::Start(LogCallback logCb, BatteryCallback batteryCb) {
    if (m_running.load()) return;

    m_logCallback = std::move(logCb);
    m_batteryCallback = std::move(batteryCb);

    m_running.store(true);
    m_workerThread = std::thread(&BatteryMonitor::WorkerLoop, this);
}

void BatteryMonitor::Stop() {
    if (!m_running.load()) return;

    m_running.store(false);
    if (m_workerThread.joinable()) {
        m_workerThread.join();
    }
    m_lastReportedLevel = -1;
    m_cachedDeviceId.clear();
    if (m_batteryCallback) {
        m_batteryCallback(L"", -1);
    }
}

void BatteryMonitor::WorkerLoop() {
    auto& loc = Localization::Instance();
    if (m_logCallback) m_logCallback(loc.Get(StringId::LogBatteryActive), LevelOf(StringId::LogBatteryActive));

#if HAS_WINRT_BLE
    try {
        winrt::init_apartment(winrt::apartment_type::multi_threaded);
    } catch (...) {}
#endif

    // Initial 1s delay
    for (int i = 0; i < 10 && m_running.load(); ++i) {
        Sleep(100);
    }

    while (m_running.load()) {
        bool connected = PollBattery();

        // If connected, check every 60s; if searching/disconnected, check every 4s
        int sleepTicks = connected ? 120 : 8; // 500ms intervals
        for (int i = 0; i < sleepTicks && m_running.load(); ++i) {
            Sleep(500);
        }
    }
}

bool BatteryMonitor::PollBattery() {
#if HAS_WINRT_BLE
    try {
        namespace ble = winrt::Windows::Devices::Bluetooth::GenericAttributeProfile;
        namespace dev_enum = winrt::Windows::Devices::Enumeration;
        namespace streams = winrt::Windows::Storage::Streams;
        namespace bt = winrt::Windows::Devices::Bluetooth;

        auto readFromId = [this](const std::wstring& id) -> bool {
            try {
                auto service = ble::GattDeviceService::FromIdAsync(id).get();
                if (!service) return false;

                // 1. Verify physical connection status
                auto dev = service.Device();
                if (!dev || dev.ConnectionStatus() != bt::BluetoothConnectionStatus::Connected) {
                    return false;
                }

                auto charResult = service.GetCharacteristicsForUuidAsync(ble::GattCharacteristicUuids::BatteryLevel()).get();
                if (charResult.Status() == ble::GattCommunicationStatus::Success && charResult.Characteristics().Size() > 0) {
                    auto ch = charResult.Characteristics().GetAt(0);

                    // 2. Uncached read forces genuine over-the-air communication
                    auto valResult = ch.ReadValueAsync(bt::BluetoothCacheMode::Uncached).get();
                    if (valResult.Status() == ble::GattCommunicationStatus::Success) {
                        auto reader = streams::DataReader::FromBuffer(valResult.Value());
                        uint8_t level = reader.ReadByte();

                        std::wstring devName = dev.Name().c_str();
                        if (devName.empty()) devName = L"8BitDo Ultimate 2C Wireless";

                        if (m_batteryCallback) {
                            m_batteryCallback(devName, static_cast<int>(level));
                        }
                        if (m_lastReportedLevel != static_cast<int>(level)) {
                            m_lastReportedLevel = static_cast<int>(level);
                            if (m_logCallback) {
                                wchar_t levelMsg[256];
                                swprintf_s(levelMsg, Localization::Instance().Get(StringId::LogBatteryLevel).c_str(),
                                           devName.c_str(), static_cast<int>(level));
                                m_logCallback(levelMsg, LevelOf(StringId::LogBatteryLevel));
                            }
                        }
                        return true;
                    }
                }
            } catch (...) {}
            return false;
        };

        // Try cached device first
        if (!m_cachedDeviceId.empty()) {
            if (readFromId(m_cachedDeviceId)) {
                return true;
            }
            m_cachedDeviceId.clear();
        }

        auto selector = ble::GattDeviceService::GetDeviceSelectorFromUuid(ble::GattServiceUuids::Battery());
        auto devices = dev_enum::DeviceInformation::FindAllAsync(selector).get();

        for (uint32_t i = 0; i < devices.Size(); ++i) {
            auto dev = devices.GetAt(i);
            std::wstring name = dev.Name().c_str();
            std::wstring id = dev.Id().c_str();

            std::wstring lowerName = name;
            std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), [](wchar_t c) { return (wchar_t)std::towlower(c); });
            std::wstring lowerId = id;
            std::transform(lowerId.begin(), lowerId.end(), lowerId.begin(), [](wchar_t c) { return (wchar_t)std::towlower(c); });

            // Detect 8BitDo hardware by name or Vendor ID (0x2DC8)
            if (lowerName.find(L"8bitdo") != std::wstring::npos || lowerId.find(L"2dc8") != std::wstring::npos) {
                if (readFromId(id)) {
                    m_cachedDeviceId = id;
                    return true;
                }
            }
        }
    } catch (...) {}

    // No active connected 8BitDo battery service found
    if (m_lastReportedLevel != -1) {
        m_lastReportedLevel = -1;
    }
    if (m_batteryCallback) {
        m_batteryCallback(L"", -1);
    }
    return false;
#else
    if (m_batteryCallback) {
        m_batteryCallback(L"", -1);
    }
    return false;
#endif
}

} // namespace Ultimate2CFixer
