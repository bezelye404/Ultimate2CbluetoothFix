#pragma once

#include <string>

namespace Ultimate2CFixer {

enum class StringId {
    AppTitle,
    StatusTitle,
    BatteryTitle,
    StartBtn,
    StopBtn,
    LogsTitle,
    ClearBtn,
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
    StatusStopped,
    NoDevice,
    Idle,
    WaitingReconnect,
    LogAppReady,
    LogServicesStarting,
    LogServicesStopping,
    LogMapperStart,
    LogMapperDisconnected,
    LogBatteryActive,
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
    LogHideNeedsReconnect,
    HideInstallBalloon,
    DisconnectOnExit,
    LogVirtualPadPlayer,
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

// How a console line is shown: neutral, good (green), bad (red) or critical, needs the user's attention (yellow).
enum class LogLevel {
    Info,
    Good,
    Bad,
    Critical
};

// The level a message is shown with.
LogLevel LevelOf(StringId id);

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
