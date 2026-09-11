#pragma once

#include <string>

namespace BitDoFixer {

enum class StringId {
    AppTitle,
    StatusTitle,
    BatteryTitle,
    ModeDesc,
    StartBtn,
    StopBtn,
    TrayBtn,
    LogsTitle,
    ClearBtn,
    Footer,
    TrayOpen,
    TrayExit,
    StatusSearching,
    StatusConnected,
    StatusDisconnected,
    StatusStopped,
    NoDevice,
    Idle,
    WaitingReconnect,
    LogAppReady,
    LogServicesStarting,
    LogServicesStopping,
    LogMapperStart,
    LogMapperReady,
    LogMapperDisconnected,
    LogBatteryActive,
    LogBatteryError
};

class Localization {
public:
    static Localization& Instance() {
        static Localization instance;
        return instance;
    }

    bool IsEnglish() const { return m_isEnglish; }
    void ToggleLanguage() { m_isEnglish = !m_isEnglish; }

    std::wstring Get(StringId id) const;

private:
    Localization() : m_isEnglish(true) {}
    bool m_isEnglish;
};

} // namespace BitDoFixer
