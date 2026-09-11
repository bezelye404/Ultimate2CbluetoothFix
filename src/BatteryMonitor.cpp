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

namespace BitDoFixer {

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
}

void BatteryMonitor::WorkerLoop() {
    auto& loc = Localization::Instance();
    if (m_logCallback) m_logCallback(loc.Get(StringId::LogBatteryActive));

#if HAS_WINRT_BLE
    try {
        winrt::init_apartment(winrt::apartment_type::multi_threaded);
    } catch (...) {}
#endif

    // Initial 2s delay
    for (int i = 0; i < 20 && m_running.load(); ++i) {
        Sleep(100);
    }

    while (m_running.load()) {
        PollBattery();

        // 300s interval (check m_running every 500ms)
        for (int i = 0; i < 600 && m_running.load(); ++i) {
            Sleep(500);
        }
    }
}

void BatteryMonitor::PollBattery() {
#if HAS_WINRT_BLE
    try {
        namespace ble = winrt::Windows::Devices::Bluetooth::GenericAttributeProfile;
        namespace dev_enum = winrt::Windows::Devices::Enumeration;
        namespace streams = winrt::Windows::Storage::Streams;

        auto readFromId = [this](const std::wstring& id) -> bool {
            try {
                auto service = ble::GattDeviceService::FromIdAsync(id).get();
                if (!service) return false;

                auto charResult = service.GetCharacteristicsForUuidAsync(ble::GattCharacteristicUuids::BatteryLevel()).get();
                if (charResult.Status() == ble::GattCommunicationStatus::Success && charResult.Characteristics().Size() > 0) {
                    auto ch = charResult.Characteristics().GetAt(0);
                    auto valResult = ch.ReadValueAsync().get();
                    if (valResult.Status() == ble::GattCommunicationStatus::Success) {
                        auto reader = streams::DataReader::FromBuffer(valResult.Value());
                        uint8_t level = reader.ReadByte();

                        std::wstring devName = service.Device() ? service.Device().Name().c_str() : L"8BitDo";
                        if (m_batteryCallback) {
                            m_batteryCallback(devName, static_cast<int>(level));
                        }
                        if (m_logCallback) {
                            m_logCallback(devName + L" Battery: " + std::to_wstring(level) + L"%");
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
                return;
            }
            m_cachedDeviceId.clear();
        }

        auto selector = ble::GattDeviceService::GetDeviceSelectorFromUuid(ble::GattServiceUuids::Battery());
        auto devices = dev_enum::DeviceInformation::FindAllAsync(selector).get();

        for (uint32_t i = 0; i < devices.Size(); ++i) {
            auto dev = devices.GetAt(i);
            std::wstring name = dev.Name().c_str();

            std::wstring lower = name;
            std::transform(lower.begin(), lower.end(), lower.begin(), [](wchar_t c) { return (wchar_t)std::towlower(c); });

            if (lower.find(L"8bitdo") != std::wstring::npos) {
                std::wstring devId = dev.Id().c_str();
                if (readFromId(devId)) {
                    m_cachedDeviceId = devId;
                    break;
                }
            }
        }
    } catch (const std::exception& ex) {
        if (m_logCallback) {
            std::string msg = ex.what();
            m_logCallback(L"Battery monitor exception: " + std::wstring(msg.begin(), msg.end()));
        }
    } catch (...) {
        if (m_logCallback) {
            m_logCallback(Localization::Instance().Get(StringId::LogBatteryError));
        }
    }
#else
    if (m_logCallback) {
        m_logCallback(L"Notice: C++/WinRT headers not present in current compiler environment; battery monitor stub active.");
    }
#endif
}

} // namespace BitDoFixer
