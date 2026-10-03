#include "Localization.h"

namespace Ultimate2CFixer {

std::wstring Localization::Get(StringId id) const {
    if (m_isEnglish) {
        switch (id) {
            case StringId::AppTitle: return L"Ultimate2CFixer";
            case StringId::StatusTitle: return L"CONTROLLER STATUS";
            case StringId::BatteryTitle: return L"BATTERY";
            case StringId::ModeDesc: return L"DirectInput -> XInput (Xbox 360)";
            case StringId::StartBtn: return L"Start Service";
            case StringId::StopBtn: return L"Stop Service";
            case StringId::TrayBtn: return L"Minimize to Tray";
            case StringId::LogsTitle: return L"Terminal & Diagnostics";
            case StringId::ClearBtn: return L"Clear";
            case StringId::Footer: return L"";
            case StringId::MinimizeOnClose: return L"Minimize to tray on close";
            case StringId::SettingsTitle: return L"SETTINGS";
            case StringId::StartWithWindows: return L"Start with Windows";
            case StringId::AutoStartService: return L"Auto-start service on launch";
            case StringId::LowBatteryNotification: return L"Low battery notification (<=15%)";
            case StringId::NintendoMode: return L"Nintendo Mode";
            case StringId::HairTrigger: return L"Hair Trigger (instant pull)";
            case StringId::PollingRateLabel: return L"Polling Rate";
            case StringId::CurveLabel: return L"Stick Curve";
            case StringId::CurveLinear: return L"Linear (1:1)";
            case StringId::CurveSmooth: return L"Smooth Aim";
            case StringId::CurveAggressive: return L"Aggressive";
            case StringId::DriverReadyFirstRun: return L"Gamepad driver (ViGEmBus) detected and ready. Have fun!";
            case StringId::DriverMissing: return L"Gamepad driver (ViGEmBus) is required to play.";
            case StringId::InstallDriverBtn: return L"Install Driver";
            case StringId::DriverDownloading: return L"Downloading driver... %d%%";
            case StringId::DriverInstalling: return L"Starting installer... Please accept the Windows prompt.";
            case StringId::DriverSuccess: return L"Driver installed successfully! Ready to connect.";
            case StringId::DriverFailed: return L"Driver setup cancelled or could not be completed.";
            case StringId::DeadzoneLabel: return L"Stick Drift / Deadzone";
            case StringId::DeadzoneOff: return L"Off (0%)";
            case StringId::DeadzoneLow: return L"Low (8%)";
            case StringId::DeadzoneNormal: return L"Normal (12%)";
            case StringId::DeadzoneHigh: return L"High (20%)";
            case StringId::SettingsBack: return L"Back";
            case StringId::LiveInputTitle: return L"LIVE INPUT TEST";
            case StringId::LowBatteryAlertTitle: return L"Ultimate2C Battery Low";
            case StringId::LowBatteryAlertMsg: return L"Battery is at %d%%. Please charge your controller.";
            case StringId::TrayOpen: return L"Open";
            case StringId::TrayExit: return L"Exit";
            case StringId::StatusSearching: return L"Searching for controller...";
            case StringId::StatusConnected: return L"Connected & Active";
            case StringId::StatusDisconnected: return L"Disconnected";
            case StringId::StatusStopped: return L"Service Stopped";
            case StringId::NoDevice: return L"No Device Detected";
            case StringId::Idle: return L"Idle";
            case StringId::WaitingReconnect: return L"Waiting for controller...";
            case StringId::LogAppReady: return L"Application ready.";
            case StringId::LogServicesStarting: return L"Starting controller and battery services...";
            case StringId::LogServicesStopping: return L"Stopping services...";
            case StringId::LogMapperStart: return L"Controller service started.";
            case StringId::LogMapperReady: return L"Controller connected and ready.";
            case StringId::LogMapperDisconnected: return L"Controller disconnected. Searching...";
            case StringId::LogBatteryActive: return L"Battery monitor active.";
            case StringId::LogBatteryError: return L"Battery monitor scan warning.";
            default: return L"";
        }
    } else {
        switch (id) {
            case StringId::AppTitle: return L"Ultimate2CFixer";
            case StringId::StatusTitle: return L"KONTROLC\x00DC DURUMU";
            case StringId::BatteryTitle: return L"P\x0130L";
            case StringId::ModeDesc: return L"DirectInput -> XInput (Xbox 360)";
            case StringId::StartBtn: return L"Servisi Ba\x015Flat";
            case StringId::StopBtn: return L"Servisi Durdur";
            case StringId::TrayBtn: return L"Tepsiye K\x00FC\x00E7\x00FClt";
            case StringId::LogsTitle: return L"Terminal ve Tan\x0131lama";
            case StringId::ClearBtn: return L"Temizle";
            case StringId::Footer: return L"";
            case StringId::MinimizeOnClose: return L"Kapat\x0131ld\x0131\x011F\x0131nda tepsiye k\x00FC\x00E7\x00FClt";
            case StringId::SettingsTitle: return L"AYARLAR";
            case StringId::StartWithWindows: return L"Windows ile ba\x015Flat";
            case StringId::AutoStartService: return L"A\x00E7\x0131l\x0131\x015Fta servisi otomatik ba\x015Flat";
            case StringId::LowBatteryNotification: return L"D\x00FC\x015F\x00FCk pil bildirimi (<=\x002515)";
            case StringId::NintendoMode: return L"Nintendo Mode";
            case StringId::HairTrigger: return L"Hair Trigger (an\x0131nda tetik)";
            case StringId::PollingRateLabel: return L"Yoklama H\x0131z\x0131 (Polling Rate)";
            case StringId::CurveLabel: return L"Stick E\x011Frisi";
            case StringId::CurveLinear: return L"Lineer (1:1)";
            case StringId::CurveSmooth: return L"Yumu\x015Fak Ni\x015Fan (Smooth)";
            case StringId::CurveAggressive: return L"Agresif";
            case StringId::DriverReadyFirstRun: return L"Gerekli gamepad s\x00FCr\x00FCc\x00FCs\x00FC (ViGEmBus) sisteminizde haz\x0131r. \x0130yi oyunlar!";
            case StringId::DriverMissing: return L"Oynamak i\x00E7in gamepad s\x00FCr\x00FCc\x00FCs\x00FC (ViGEmBus) gerekiyor.";
            case StringId::InstallDriverBtn: return L"S\x00FCr\x00FCc\x00FCy\x00FC Kur";
            case StringId::DriverDownloading: return L"S\x00FCr\x00FCc\x00FC indiriliyor... \x0025%d";
            case StringId::DriverInstalling: return L"Kurulum ba\x015Flat\x0131l\x0131yor... L\x00FCtfen ekrandaki Windows onay\x0131n\x0131 kabul edin.";
            case StringId::DriverSuccess: return L"S\x00FCr\x00FCc\x00FC ba\x015Far\x0131yla kuruldu! Ba\x011Flanmaya haz\x0131r.";
            case StringId::DriverFailed: return L"S\x00FCr\x00FCc\x00FC kurulumu iptal edildi veya tamamlanamad\x0131.";
            case StringId::DeadzoneLabel: return L"Stick Drift / \x00D6l\x00FC B\x00F6lge";
            case StringId::DeadzoneOff: return L"Kapal\x0131 (\x00250)";
            case StringId::DeadzoneLow: return L"D\x00FC\x015F\x00FCk (\x00258)";
            case StringId::DeadzoneNormal: return L"Normal (\x002512)";
            case StringId::DeadzoneHigh: return L"Y\x00FCksek (\x002520)";
            case StringId::SettingsBack: return L"Geri";
            case StringId::LiveInputTitle: return L"CANLI G\x0130RD\x0130 TEST\x0130";
            case StringId::LowBatteryAlertTitle: return L"Ultimate2C Pili D\x00FC\x015Ft\x00FC";
            case StringId::LowBatteryAlertMsg: return L"Pil seviyesi \x0025%d. L\x00FCtfen kolu \x015Farj edin.";
            case StringId::TrayOpen: return L"A\x00E7";
            case StringId::TrayExit: return L"\x00C7\x0131k\x0131\x015F";
            case StringId::StatusSearching: return L"Kontrolc\x00FC aran\x0131yor...";
            case StringId::StatusConnected: return L"Ba\x011Fl\x0131 ve Aktif";
            case StringId::StatusDisconnected: return L"Ba\x011Flant\x0131 Koptu";
            case StringId::StatusStopped: return L"Servis Durduruldu";
            case StringId::NoDevice: return L"Cihaz Alg\x0131lanmad\x0131";
            case StringId::Idle: return L"Bo\x015Fta";
            case StringId::WaitingReconnect: return L"Kolun a\x00E7\x0131lmas\x0131 bekleniyor...";
            case StringId::LogAppReady: return L"Uygulama haz\x0131r.";
            case StringId::LogServicesStarting: return L"Servisler ba\x015Flat\x0131l\x0131yor...";
            case StringId::LogServicesStopping: return L"Servisler durduruluyor...";
            case StringId::LogMapperStart: return L"Kontrolc\x00FC servisi ba\x015Flat\x0131ld\x0131.";
            case StringId::LogMapperReady: return L"Kontrolc\x00FC ba\x011Flan\x0131ld\x0131 ve haz\x0131r.";
            case StringId::LogMapperDisconnected: return L"Ba\x011Flant\x0131 koptu. Yeniden ba\x011Flan\x0131l\x0131yor...";
            case StringId::LogBatteryActive: return L"Pil monit\x00F6r\x00FC aktif.";
            case StringId::LogBatteryError: return L"Pil tarama uyar\x0131s\x0131.";
            default: return L"";
        }
    }
}

} // namespace Ultimate2CFixer
