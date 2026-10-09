#include "Localization.h"

namespace Ultimate2CFixer {

namespace {

// Strings are written as direct UTF-8 characters (the project builds with /utf-8).
// Do not use \x escapes here: they are greedy and swallow following hex-looking characters.

std::wstring GetEnglish(StringId id) {
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
        case StringId::HideRealController: return L"Hide original controller from games (prevents double inputs)";
        case StringId::HideInstallTitle: return L"Install hiding driver";
        case StringId::HideInstallPrompt: return L"Hiding the original controller needs the free HidHide driver (about 8 MB, downloaded from its official release page). Download and install it now?";
        case StringId::LogHideActive: return L"Original controller hidden from games.";
        case StringId::LogHideAlready: return L"Original controller was already hidden from games.";
        case StringId::LogHideMissing: return L"Hiding driver (HidHide) is not installed. Games may see two controllers.";
        case StringId::LogHideNoDevice: return L"Could not find the original controller to hide.";
        case StringId::LogHideCustomSetup: return L"Hiding skipped: your existing HidHide setup uses a custom layout and was left untouched.";
        case StringId::LogHideFailed: return L"Could not hide the original controller. Games may see two controllers.";
        case StringId::LogHideRestored: return L"Original controller is visible to games again.";
        case StringId::LogHideSuspended: return L"Controller could not be read while hidden. Hiding is turned off for this session.";
        case StringId::LogHideRecovered: return L"Restored controller visibility left over from a previous session.";
        case StringId::LogHideApplyNext: return L"This change applies the next time the service starts.";
        case StringId::LogHideInstalling: return L"Downloading hiding driver... Please accept the Windows prompt when it appears.";
        case StringId::LogHideInstalled: return L"Hiding driver installed.";
        case StringId::LogHideInstallFailed: return L"Hiding driver could not be downloaded. Please check your internet connection.";
        case StringId::LogHideInstallUnverified: return L"Downloaded file could not be verified and was not run.";
        case StringId::LogHideInstallCancelled: return L"Hiding driver setup was cancelled.";
        case StringId::LogHideInstallRestart: return L"Hiding driver installed. Restart Windows to finish, then start the service again.";
        case StringId::BatteryLowStatus: return L"Low Battery - Please Recharge";
        case StringId::BatteryModerateStatus: return L"Wireless - Moderate Level";
        case StringId::BatteryHealthyStatus: return L"Wireless - Healthy Level";
        default: return L"";
    }
}

std::wstring GetTurkish(StringId id) {
    switch (id) {
        case StringId::AppTitle: return L"Ultimate2CFixer";
        case StringId::StatusTitle: return L"KONTROLCÜ DURUMU";
        case StringId::BatteryTitle: return L"PİL";
        case StringId::ModeDesc: return L"DirectInput -> XInput (Xbox 360)";
        case StringId::StartBtn: return L"Servisi Başlat";
        case StringId::StopBtn: return L"Servisi Durdur";
        case StringId::TrayBtn: return L"Tepsiye Küçült";
        case StringId::LogsTitle: return L"Terminal ve Tanılama";
        case StringId::ClearBtn: return L"Temizle";
        case StringId::Footer: return L"";
        case StringId::MinimizeOnClose: return L"Kapatıldığında tepsiye küçült";
        case StringId::SettingsTitle: return L"AYARLAR";
        case StringId::StartWithWindows: return L"Windows ile başlat";
        case StringId::AutoStartService: return L"Açılışta servisi otomatik başlat";
        case StringId::LowBatteryNotification: return L"Düşük pil bildirimi (<=%15)";
        case StringId::NintendoMode: return L"Nintendo Mode";
        case StringId::HairTrigger: return L"Hair Trigger (anında tetik)";
        case StringId::PollingRateLabel: return L"Yoklama Hızı (Polling Rate)";
        case StringId::CurveLabel: return L"Stick Eğrisi";
        case StringId::CurveLinear: return L"Lineer (1:1)";
        case StringId::CurveSmooth: return L"Yumuşak Nişan (Smooth)";
        case StringId::CurveAggressive: return L"Agresif";
        case StringId::DriverReadyFirstRun: return L"Gerekli gamepad sürücüsü (ViGEmBus) sisteminizde hazır. İyi oyunlar!";
        case StringId::DriverMissing: return L"Oynamak için gamepad sürücüsü (ViGEmBus) gerekiyor.";
        case StringId::InstallDriverBtn: return L"Sürücüyü Kur";
        case StringId::DriverDownloading: return L"Sürücü indiriliyor... %%%d";
        case StringId::DriverInstalling: return L"Kurulum başlatılıyor... Lütfen ekrandaki Windows onayını kabul edin.";
        case StringId::DriverSuccess: return L"Sürücü başarıyla kuruldu! Bağlanmaya hazır.";
        case StringId::DriverFailed: return L"Sürücü kurulumu iptal edildi veya tamamlanamadı.";
        case StringId::DeadzoneLabel: return L"Stick Drift / Ölü Bölge";
        case StringId::DeadzoneOff: return L"Kapalı (%0)";
        case StringId::DeadzoneLow: return L"Düşük (%8)";
        case StringId::DeadzoneNormal: return L"Normal (%12)";
        case StringId::DeadzoneHigh: return L"Yüksek (%20)";
        case StringId::SettingsBack: return L"Geri";
        case StringId::LiveInputTitle: return L"CANLI GİRDİ TESTİ";
        case StringId::LowBatteryAlertTitle: return L"Ultimate2C Pili Düştü";
        case StringId::LowBatteryAlertMsg: return L"Pil seviyesi %%%d. Lütfen kolu şarj edin.";
        case StringId::TrayOpen: return L"Aç";
        case StringId::TrayExit: return L"Çıkış";
        case StringId::StatusSearching: return L"Kontrolcü aranıyor...";
        case StringId::StatusConnected: return L"Bağlı ve Aktif";
        case StringId::StatusDisconnected: return L"Bağlantı Koptu";
        case StringId::StatusStopped: return L"Servis Durduruldu";
        case StringId::NoDevice: return L"Cihaz Algılanmadı";
        case StringId::Idle: return L"Boşta";
        case StringId::WaitingReconnect: return L"Kolun açılması bekleniyor...";
        case StringId::LogAppReady: return L"Uygulama hazır.";
        case StringId::LogServicesStarting: return L"Servisler başlatılıyor...";
        case StringId::LogServicesStopping: return L"Servisler durduruluyor...";
        case StringId::LogMapperStart: return L"Kontrolcü servisi başlatıldı.";
        case StringId::LogMapperReady: return L"Kontrolcü bağlanıldı ve hazır.";
        case StringId::LogMapperDisconnected: return L"Bağlantı koptu. Yeniden bağlanılıyor...";
        case StringId::LogBatteryActive: return L"Pil monitörü aktif.";
        case StringId::LogBatteryError: return L"Pil tarama uyarısı.";
        case StringId::HideRealController: return L"Orijinal kontrolcüyü oyunlardan gizle (çift girdiyi önler)";
        case StringId::HideInstallTitle: return L"Gizleme sürücüsünü kur";
        case StringId::HideInstallPrompt: return L"Orijinal kontrolcüyü gizlemek için ücretsiz HidHide sürücüsü gerekiyor (yaklaşık 8 MB, resmi yayın sayfasından indirilir). Şimdi indirilip kurulsun mu?";
        case StringId::LogHideActive: return L"Orijinal kontrolcü oyunlardan gizlendi.";
        case StringId::LogHideAlready: return L"Orijinal kontrolcü zaten oyunlardan gizliydi.";
        case StringId::LogHideMissing: return L"Gizleme sürücüsü (HidHide) kurulu değil. Oyunlar iki kontrolcü görebilir.";
        case StringId::LogHideNoDevice: return L"Gizlenecek orijinal kontrolcü bulunamadı.";
        case StringId::LogHideCustomSetup: return L"Gizleme atlandı: mevcut HidHide ayarlarınız özel bir düzende olduğu için değiştirilmedi.";
        case StringId::LogHideFailed: return L"Orijinal kontrolcü gizlenemedi. Oyunlar iki kontrolcü görebilir.";
        case StringId::LogHideRestored: return L"Orijinal kontrolcü oyunlara yeniden görünür.";
        case StringId::LogHideSuspended: return L"Kontrolcü gizliyken okunamadı. Gizleme bu oturum için kapatıldı.";
        case StringId::LogHideRecovered: return L"Önceki oturumdan kalan kontrolcü gizleme ayarı geri alındı.";
        case StringId::LogHideApplyNext: return L"Bu değişiklik servis bir sonraki başlatılışında uygulanır.";
        case StringId::LogHideInstalling: return L"Gizleme sürücüsü indiriliyor... Windows onayı çıkınca lütfen kabul edin.";
        case StringId::LogHideInstalled: return L"Gizleme sürücüsü kuruldu.";
        case StringId::LogHideInstallFailed: return L"Gizleme sürücüsü indirilemedi. Lütfen internet bağlantınızı kontrol edin.";
        case StringId::LogHideInstallUnverified: return L"İndirilen dosya doğrulanamadı ve çalıştırılmadı.";
        case StringId::LogHideInstallCancelled: return L"Gizleme sürücüsü kurulumu iptal edildi.";
        case StringId::LogHideInstallRestart: return L"Gizleme sürücüsü kuruldu. Tamamlamak için Windows'u yeniden başlatın, sonra servisi tekrar başlatın.";
        case StringId::BatteryLowStatus: return L"Düşük Pil - Lütfen Şarj Edin";
        case StringId::BatteryModerateStatus: return L"Kablosuz - Orta Seviye";
        case StringId::BatteryHealthyStatus: return L"Kablosuz - İyi Seviye";
        default: return L"";
    }
}

std::wstring GetSpanish(StringId id) {
    switch (id) {
        case StringId::AppTitle: return L"Ultimate2CFixer";
        case StringId::StatusTitle: return L"ESTADO DEL MANDO";
        case StringId::BatteryTitle: return L"BATERÍA";
        case StringId::ModeDesc: return L"DirectInput -> XInput (Xbox 360)";
        case StringId::StartBtn: return L"Iniciar servicio";
        case StringId::StopBtn: return L"Detener servicio";
        case StringId::TrayBtn: return L"Minimizar a la bandeja";
        case StringId::LogsTitle: return L"Terminal y diagnóstico";
        case StringId::ClearBtn: return L"Borrar";
        case StringId::Footer: return L"";
        case StringId::MinimizeOnClose: return L"Minimizar a la bandeja al cerrar";
        case StringId::SettingsTitle: return L"AJUSTES";
        case StringId::StartWithWindows: return L"Iniciar con Windows";
        case StringId::AutoStartService: return L"Iniciar el servicio automáticamente al abrir";
        case StringId::LowBatteryNotification: return L"Aviso de batería baja (<=15%)";
        case StringId::NintendoMode: return L"Modo Nintendo";
        case StringId::HairTrigger: return L"Gatillo instantáneo (Hair Trigger)";
        case StringId::PollingRateLabel: return L"Frecuencia de sondeo";
        case StringId::CurveLabel: return L"Curva del stick";
        case StringId::CurveLinear: return L"Lineal (1:1)";
        case StringId::CurveSmooth: return L"Apuntado suave";
        case StringId::CurveAggressive: return L"Agresiva";
        case StringId::DriverReadyFirstRun: return L"El controlador del gamepad (ViGEmBus) está detectado y listo. ¡A jugar!";
        case StringId::DriverMissing: return L"Se necesita el controlador del gamepad (ViGEmBus) para jugar.";
        case StringId::InstallDriverBtn: return L"Instalar controlador";
        case StringId::DriverDownloading: return L"Descargando controlador... %d%%";
        case StringId::DriverInstalling: return L"Iniciando instalador... Acepta el aviso de Windows.";
        case StringId::DriverSuccess: return L"¡Controlador instalado correctamente! Listo para conectar.";
        case StringId::DriverFailed: return L"La instalación del controlador se canceló o no pudo completarse.";
        case StringId::DeadzoneLabel: return L"Zona muerta";
        case StringId::DeadzoneOff: return L"Desactivada (0%)";
        case StringId::DeadzoneLow: return L"Baja (8%)";
        case StringId::DeadzoneNormal: return L"Normal (12%)";
        case StringId::DeadzoneHigh: return L"Alta (20%)";
        case StringId::SettingsBack: return L"Atrás";
        case StringId::LiveInputTitle: return L"PRUEBA DE ENTRADA EN VIVO";
        case StringId::LowBatteryAlertTitle: return L"Batería baja del Ultimate2C";
        case StringId::LowBatteryAlertMsg: return L"La batería está al %d%%. Carga tu mando.";
        case StringId::TrayOpen: return L"Abrir";
        case StringId::TrayExit: return L"Salir";
        case StringId::StatusSearching: return L"Buscando mando...";
        case StringId::StatusConnected: return L"Conectado y activo";
        case StringId::StatusDisconnected: return L"Desconectado";
        case StringId::StatusStopped: return L"Servicio detenido";
        case StringId::NoDevice: return L"Ningún dispositivo detectado";
        case StringId::Idle: return L"Inactivo";
        case StringId::WaitingReconnect: return L"Esperando al mando...";
        case StringId::LogAppReady: return L"Aplicación lista.";
        case StringId::LogServicesStarting: return L"Iniciando servicios del mando y de la batería...";
        case StringId::LogServicesStopping: return L"Deteniendo servicios...";
        case StringId::LogMapperStart: return L"Servicio del mando iniciado.";
        case StringId::LogMapperReady: return L"Mando conectado y listo.";
        case StringId::LogMapperDisconnected: return L"Mando desconectado. Buscando...";
        case StringId::LogBatteryActive: return L"Monitor de batería activo.";
        case StringId::LogBatteryError: return L"Aviso en el análisis de la batería.";
        case StringId::HideRealController: return L"Ocultar el mando original de los juegos (evita entradas dobles)";
        case StringId::HideInstallTitle: return L"Instalar controlador de ocultación";
        case StringId::HideInstallPrompt: return L"Para ocultar el mando original se necesita el controlador gratuito HidHide (unos 8 MB, descargado desde su página oficial de versiones). ¿Descargarlo e instalarlo ahora?";
        case StringId::LogHideActive: return L"Mando original oculto para los juegos.";
        case StringId::LogHideAlready: return L"El mando original ya estaba oculto para los juegos.";
        case StringId::LogHideMissing: return L"El controlador de ocultación (HidHide) no está instalado. Los juegos pueden ver dos mandos.";
        case StringId::LogHideNoDevice: return L"No se encontró el mando original para ocultarlo.";
        case StringId::LogHideCustomSetup: return L"Ocultación omitida: tu configuración actual de HidHide usa un diseño personalizado y no se modificó.";
        case StringId::LogHideFailed: return L"No se pudo ocultar el mando original. Los juegos pueden ver dos mandos.";
        case StringId::LogHideRestored: return L"El mando original vuelve a ser visible para los juegos.";
        case StringId::LogHideSuspended: return L"No se pudo leer el mando mientras estaba oculto. La ocultación se desactivó en esta sesión.";
        case StringId::LogHideRecovered: return L"Se restauró la visibilidad del mando que quedó de una sesión anterior.";
        case StringId::LogHideApplyNext: return L"Este cambio se aplicará la próxima vez que se inicie el servicio.";
        case StringId::LogHideInstalling: return L"Descargando controlador de ocultación... Acepta el aviso de Windows cuando aparezca.";
        case StringId::LogHideInstalled: return L"Controlador de ocultación instalado.";
        case StringId::LogHideInstallFailed: return L"No se pudo descargar el controlador de ocultación. Comprueba tu conexión a Internet.";
        case StringId::LogHideInstallUnverified: return L"No se pudo verificar el archivo descargado y no se ejecutó.";
        case StringId::LogHideInstallCancelled: return L"Se canceló la instalación del controlador de ocultación.";
        case StringId::LogHideInstallRestart: return L"Controlador de ocultación instalado. Reinicia Windows para terminar y luego inicia el servicio de nuevo.";
        case StringId::BatteryLowStatus: return L"Batería baja - Recarga el mando";
        case StringId::BatteryModerateStatus: return L"Inalámbrico - Nivel medio";
        case StringId::BatteryHealthyStatus: return L"Inalámbrico - Nivel bueno";
        default: return L"";
    }
}

} // namespace

const wchar_t* Localization::Code() const {
    switch (m_language) {
        case Language::Turkish: return L"TR";
        case Language::Spanish: return L"ES";
        default: return L"EN";
    }
}

std::wstring Localization::Get(StringId id) const {
    switch (m_language) {
        case Language::Turkish: return GetTurkish(id);
        case Language::Spanish: return GetSpanish(id);
        default: return GetEnglish(id);
    }
}

} // namespace Ultimate2CFixer
