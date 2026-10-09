#pragma once

#include <string>

namespace Ultimate2CFixer {

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
    MinimizeOnClose,
    SettingsTitle,
    StartWithWindows,
    AutoStartService,
    LowBatteryNotification,
    NintendoMode,
    HairTrigger,
    PollingRateLabel,
    CurveLabel,
    CurveLinear,
    CurveSmooth,
    CurveAggressive,
    DriverReadyFirstRun,
    DriverMissing,
    InstallDriverBtn,
    DriverDownloading,
    DriverInstalling,
    DriverSuccess,
    DriverFailed,
    DeadzoneLabel,
    DeadzoneOff,
    DeadzoneLow,
    DeadzoneNormal,
    DeadzoneHigh,
    SettingsBack,
    LiveInputTitle,
    LowBatteryAlertTitle,
    LowBatteryAlertMsg,
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
    LogBatteryError,
    HideInstallTitle,
    HideInstallPrompt,
    LogHideActive,
    LogHideAlready,
    LogHideMissing,
    LogHideNoDevice,
    LogHideCustomSetup,
    LogHideFailed,
    LogHideRestored,
    LogHideSuspended,
    LogHideRecovered,
    LogHideRepaired,
    LogHideInstalling,
    LogHideInstalled,
    LogHideInstallFailed,
    LogHideInstallUnverified,
    LogHideInstallCancelled,
    LogHideInstallRestart,
    BatteryLowStatus,
    BatteryModerateStatus,
    BatteryHealthyStatus,
    DriverDownloadingShort,
    LogViGEmUnavailable,
    LogInputSystemFailed,
    LogControllerConnected,
    LogBatteryLevel
};

enum class Language {
    English,
    Turkish,
    Spanish
};

class Localization {
public:
    static Localization& Instance() {
        static Localization instance;
        return instance;
    }

    Language Current() const { return m_language; }
    void SetLanguage(Language language) { m_language = language; }

    // Cycles English -> Turkish -> Spanish -> English.
    void NextLanguage() {
        m_language = static_cast<Language>((static_cast<int>(m_language) + 1) % 3);
    }

    // Short code of the active language, shown on the language button.
    const wchar_t* Code() const;

    std::wstring Get(StringId id) const;

private:
    Localization() : m_language(Language::English) {}
    Language m_language;
};

} // namespace Ultimate2CFixer
