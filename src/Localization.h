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
    HideRealController,
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
    LogHideApplyNext,
    LogHideInstalling,
    LogHideInstalled,
    LogHideInstallFailed,
    LogHideInstallUnverified,
    LogHideInstallCancelled,
    LogHideInstallRestart
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

} // namespace Ultimate2CFixer
