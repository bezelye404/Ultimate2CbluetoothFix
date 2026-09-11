#include "Localization.h"

namespace BitDoFixer {

std::wstring Localization::Get(StringId id) const {
    if (m_isEnglish) {
        switch (id) {
            case StringId::AppTitle: return L"8BitDo Ultimate 2C Fixer";
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
            case StringId::DeadzoneLabel: return L"Stick Drift / Deadzone";
            case StringId::DeadzoneOff: return L"Off (0%)";
            case StringId::DeadzoneLow: return L"Low (8%)";
            case StringId::DeadzoneNormal: return L"Normal (12%)";
            case StringId::DeadzoneHigh: return L"High (20%)";
            case StringId::SettingsBack: return L"Back";
            case StringId::LiveInputTitle: return L"LIVE INPUT TEST";
            case StringId::LowBatteryAlertTitle: return L"8BitDo Battery Low";
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
            case StringId::LogAppReady: return L"Application ready (C++ Native).";
            case StringId::LogServicesStarting: return L"Starting remapper and battery services...";
            case StringId::LogServicesStopping: return L"Stopping services...";
            case StringId::LogMapperStart: return L"DirectInput -> Virtual Xbox 360 remapper started.";
            case StringId::LogMapperReady: return L"Virtual Xbox 360 controller connected and ready.";
            case StringId::LogMapperDisconnected: return L"Controller disconnected. Entering auto-reconnect scan...";
            case StringId::LogBatteryActive: return L"Battery monitor active.";
            case StringId::LogBatteryError: return L"Battery monitor scan warning.";
            default: return L"";
        }
    } else {
        switch (id) {
            case StringId::AppTitle: return L"8BitDo Ultimate 2C D\x00FCzeltici";
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
            case StringId::DeadzoneLabel: return L"Stick Drift / \x00D6l\x00FC B\x00F6lge";
            case StringId::DeadzoneOff: return L"Kapal\x0131 (\x00250)";
            case StringId::DeadzoneLow: return L"D\x00FC\x015F\x00FCk (\x00258)";
            case StringId::DeadzoneNormal: return L"Normal (\x002512)";
            case StringId::DeadzoneHigh: return L"Y\x00FCksek (\x002520)";
            case StringId::SettingsBack: return L"Geri";
            case StringId::LiveInputTitle: return L"CANLI G\x0130RD\x0130 TEST\x0130";
            case StringId::LowBatteryAlertTitle: return L"8BitDo Pili D\x00FC\x015Ft\x00FC";
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
            case StringId::LogAppReady: return L"Uygulama haz\x0131r (Yerel C++).";
            case StringId::LogServicesStarting: return L"Servisler ba\x015Flat\x0131l\x0131yor...";
            case StringId::LogServicesStopping: return L"Servisler durduruluyor...";
            case StringId::LogMapperStart: return L"DirectInput -> Sanal Xbox 360 remapper ba\x015Flat\x0131ld\x0131.";
            case StringId::LogMapperReady: return L"Sanal Xbox 360 kontrolc\x00FCs\x00FC haz\x0131r.";
            case StringId::LogMapperDisconnected: return L"Ba\x011Flant\x0131 koptu. Otomatik yeniden ba\x011Flanma moduna ge\x00E7ildi...";
            case StringId::LogBatteryActive: return L"Pil monit\x00F6r\x00FC aktif.";
            case StringId::LogBatteryError: return L"Pil tarama uyar\x0131s\x0131.";
            default: return L"";
        }
    }
}

} // namespace BitDoFixer
