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
        case StringId::StartBtn: return L"Start Service";
        case StringId::StopBtn: return L"Stop Service";
        case StringId::LogsTitle: return L"Terminal & Diagnostics";
        case StringId::ClearBtn: return L"Clear";
        case StringId::MinimizeOnClose: return L"Minimize to tray on close";
        case StringId::SettingsTitle: return L"SETTINGS";
        case StringId::StartWithWindows: return L"Start with Windows";
        case StringId::AutoStartService: return L"Auto-start service on launch";
        case StringId::LowBatteryNotification: return L"Low battery notification (<=15%)";
        case StringId::NintendoMode: return L"Nintendo Mode";
        case StringId::HairTrigger: return L"Hair Trigger (instant pull)";
        case StringId::PollingRateLabel: return L"Max Update Rate";
        case StringId::CurveLabel: return L"Stick Curve";
        case StringId::CurveLinear: return L"Linear (1:1)";
        case StringId::CurveSmooth: return L"Smooth Aim";
        case StringId::CurveAggressive: return L"Aggressive";
        case StringId::DriverReadyFirstRun: return L"Gamepad driver (ViGEmBus) detected and ready. Have fun!";
        case StringId::DriverMissing: return L"Gamepad driver (ViGEmBus) is required to play.";
        case StringId::InstallDriverBtn: return L"Install Driver";
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
        case StringId::StatusStopped: return L"Service Stopped";
        case StringId::NoDevice: return L"No Device Detected";
        case StringId::Idle: return L"Idle";
        case StringId::WaitingReconnect: return L"Waiting for controller...";
        case StringId::LogAppReady: return L"Application ready.";
        case StringId::LogServicesStarting: return L"Starting controller and battery services...";
        case StringId::LogServicesStopping: return L"Stopping services...";
        case StringId::LogMapperStart: return L"Controller service started.";
        case StringId::LogMapperDisconnected: return L"Controller disconnected. Searching...";
        case StringId::LogBatteryActive: return L"Battery monitor active.";
        case StringId::HideInstallTitle: return L"Install hiding driver";
        case StringId::HideInstallPrompt: return L"Hiding the original controller needs the free HidHide driver (about 8 MB, downloaded from its official release page). Download and install it now?";
        case StringId::LogHideActive: return L"Original controller hidden from games.";
        case StringId::LogHideAlready: return L"Original controller was already hidden from games.";
        case StringId::LogHideMissing: return L"Hiding driver (HidHide) is not installed. Games may see two controllers.";
        case StringId::LogHideNoDevice: return L"Could not find the original controller to hide.";
        case StringId::LogHideCustomSetup: return L"Hiding skipped: your existing HidHide setup uses a custom layout and was left untouched.";
        case StringId::LogHideFailed: return L"Could not hide the original controller. Games may see two controllers.";
        case StringId::LogHideRestored: return L"Original controller is visible to games again.";
        case StringId::LogHideSuspended: return L"The controller is connected but could not be reached while hidden. Hiding is paused for a minute.";
        case StringId::LogHideNeedsReconnect: return L"The controller was already connected. Turn it off and on once so browsers and games see only one controller.";
        case StringId::HideInstallBalloon: return L"Install the HidHide driver so games see only one controller. Click here to install it.";
        case StringId::DisconnectOnExit: return L"Disconnect the controller when the app closes (asks for permission)";
        case StringId::LogVirtualPadPlayer: return L"Virtual gamepad is player %d.";
        case StringId::LogHideRecovered: return L"Restored controller visibility left over from a previous session.";
        case StringId::LogHideRepaired: return L"Hiding was changed by another program and has been restored. If games still see two controllers, turn the controller off and on again.";
        case StringId::LogHideInstalling: return L"Downloading hiding driver... Please accept the Windows prompt when it appears.";
        case StringId::LogHideInstalled: return L"Hiding driver installed.";
        case StringId::LogHideInstallFailed: return L"Hiding driver could not be downloaded. Please check your internet connection.";
        case StringId::LogHideInstallUnverified: return L"Downloaded file could not be verified and was not run.";
        case StringId::LogHideInstallCancelled: return L"Hiding driver setup was cancelled.";
        case StringId::LogHideInstallRestart: return L"Hiding driver installed. Restart Windows to finish, then start the service again.";
        case StringId::BatteryLowStatus: return L"Low Battery - Please Recharge";
        case StringId::BatteryModerateStatus: return L"Wireless - Moderate Level";
        case StringId::BatteryHealthyStatus: return L"Wireless - Healthy Level";
        case StringId::DriverDownloadingShort: return L"Downloading %d%%";
        case StringId::LogViGEmUnavailable: return L"Could not connect to the gamepad driver (ViGEmBus). Try reinstalling it.";
        case StringId::LogInputSystemFailed: return L"Could not start the controller input system.";
        case StringId::LogControllerConnected: return L"%ls connected.";
        case StringId::LogBatteryLevel: return L"%ls Battery: %d%%";
        default: return L"";
    }
}

std::wstring GetTurkish(StringId id) {
    switch (id) {
        case StringId::AppTitle: return L"Ultimate2CFixer";
        case StringId::StatusTitle: return L"KONTROLCÜ DURUMU";
        case StringId::BatteryTitle: return L"PİL";
        case StringId::StartBtn: return L"Servisi Başlat";
        case StringId::StopBtn: return L"Servisi Durdur";
        case StringId::LogsTitle: return L"Terminal ve Tanılama";
        case StringId::ClearBtn: return L"Temizle";
        case StringId::MinimizeOnClose: return L"Kapatıldığında tepsiye küçült";
        case StringId::SettingsTitle: return L"AYARLAR";
        case StringId::StartWithWindows: return L"Windows ile başlat";
        case StringId::AutoStartService: return L"Açılışta servisi otomatik başlat";
        case StringId::LowBatteryNotification: return L"Düşük pil bildirimi (<=%15)";
        case StringId::NintendoMode: return L"Nintendo Mode";
        case StringId::HairTrigger: return L"Hair Trigger (anında tetik)";
        case StringId::PollingRateLabel: return L"Maksimum Güncelleme Hızı";
        case StringId::CurveLabel: return L"Stick Eğrisi";
        case StringId::CurveLinear: return L"Lineer (1:1)";
        case StringId::CurveSmooth: return L"Yumuşak Nişan (Smooth)";
        case StringId::CurveAggressive: return L"Agresif";
        case StringId::DriverReadyFirstRun: return L"Gerekli gamepad sürücüsü (ViGEmBus) sisteminizde hazır. İyi oyunlar!";
        case StringId::DriverMissing: return L"Oynamak için gamepad sürücüsü (ViGEmBus) gerekiyor.";
        case StringId::InstallDriverBtn: return L"Sürücüyü Kur";
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
        case StringId::StatusStopped: return L"Servis Durduruldu";
        case StringId::NoDevice: return L"Cihaz Algılanmadı";
        case StringId::Idle: return L"Boşta";
        case StringId::WaitingReconnect: return L"Kolun açılması bekleniyor...";
        case StringId::LogAppReady: return L"Uygulama hazır.";
        case StringId::LogServicesStarting: return L"Servisler başlatılıyor...";
        case StringId::LogServicesStopping: return L"Servisler durduruluyor...";
        case StringId::LogMapperStart: return L"Kontrolcü servisi başlatıldı.";
        case StringId::LogMapperDisconnected: return L"Bağlantı koptu. Yeniden bağlanılıyor...";
        case StringId::LogBatteryActive: return L"Pil monitörü aktif.";
        case StringId::HideInstallTitle: return L"Gizleme sürücüsünü kur";
        case StringId::HideInstallPrompt: return L"Orijinal kontrolcüyü gizlemek için ücretsiz HidHide sürücüsü gerekiyor (yaklaşık 8 MB, resmi yayın sayfasından indirilir). Şimdi indirilip kurulsun mu?";
        case StringId::LogHideActive: return L"Orijinal kontrolcü oyunlardan gizlendi.";
        case StringId::LogHideAlready: return L"Orijinal kontrolcü zaten oyunlardan gizliydi.";
        case StringId::LogHideMissing: return L"Gizleme sürücüsü (HidHide) kurulu değil. Oyunlar iki kontrolcü görebilir.";
        case StringId::LogHideNoDevice: return L"Gizlenecek orijinal kontrolcü bulunamadı.";
        case StringId::LogHideCustomSetup: return L"Gizleme atlandı: mevcut HidHide ayarlarınız özel bir düzende olduğu için değiştirilmedi.";
        case StringId::LogHideFailed: return L"Orijinal kontrolcü gizlenemedi. Oyunlar iki kontrolcü görebilir.";
        case StringId::LogHideRestored: return L"Orijinal kontrolcü oyunlara yeniden görünür.";
        case StringId::LogHideSuspended: return L"Kontrolcü bağlı ama gizliyken erişilemedi. Gizleme bir dakika duraklatıldı.";
        case StringId::LogHideNeedsReconnect: return L"Kontrolcü zaten bağlıydı. Tarayıcıların ve oyunların tek kontrolcü görmesi için kontrolcüyü bir kez kapatıp açın.";
        case StringId::HideInstallBalloon: return L"Oyunların tek kontrolcü görmesi için HidHide sürücüsünü kurun. Kurmak için buraya tıklayın.";
        case StringId::DisconnectOnExit: return L"Uygulama kapanırken kontrolcünün bağlantısını kes (izin ister)";
        case StringId::LogVirtualPadPlayer: return L"Sanal gamepad %d. oyuncu.";
        case StringId::LogHideRecovered: return L"Önceki oturumdan kalan kontrolcü gizleme ayarı geri alındı.";
        case StringId::LogHideRepaired: return L"Gizleme başka bir program tarafından değiştirildi ve yeniden uygulandı. Oyunlar hâlâ iki kontrolcü görüyorsa kontrolcüyü kapatıp açın.";
        case StringId::LogHideInstalling: return L"Gizleme sürücüsü indiriliyor... Windows onayı çıkınca lütfen kabul edin.";
        case StringId::LogHideInstalled: return L"Gizleme sürücüsü kuruldu.";
        case StringId::LogHideInstallFailed: return L"Gizleme sürücüsü indirilemedi. Lütfen internet bağlantınızı kontrol edin.";
        case StringId::LogHideInstallUnverified: return L"İndirilen dosya doğrulanamadı ve çalıştırılmadı.";
        case StringId::LogHideInstallCancelled: return L"Gizleme sürücüsü kurulumu iptal edildi.";
        case StringId::LogHideInstallRestart: return L"Gizleme sürücüsü kuruldu. Tamamlamak için Windows'u yeniden başlatın, sonra servisi tekrar başlatın.";
        case StringId::BatteryLowStatus: return L"Düşük Pil - Lütfen Şarj Edin";
        case StringId::BatteryModerateStatus: return L"Kablosuz - Orta Seviye";
        case StringId::BatteryHealthyStatus: return L"Kablosuz - İyi Seviye";
        case StringId::DriverDownloadingShort: return L"İndiriliyor %%%d";
        case StringId::LogViGEmUnavailable: return L"Gamepad sürücüsüne (ViGEmBus) bağlanılamadı. Yeniden kurmayı deneyin.";
        case StringId::LogInputSystemFailed: return L"Kontrolcü girdi sistemi başlatılamadı.";
        case StringId::LogControllerConnected: return L"%ls bağlandı.";
        case StringId::LogBatteryLevel: return L"%ls Pil: %%%d";
        default: return L"";
    }
}

std::wstring GetSpanish(StringId id) {
    switch (id) {
        case StringId::AppTitle: return L"Ultimate2CFixer";
        case StringId::StatusTitle: return L"ESTADO DEL CONTROL";
        case StringId::BatteryTitle: return L"BATERÍA";
        case StringId::StartBtn: return L"Iniciar servicio";
        case StringId::StopBtn: return L"Detener servicio";
        case StringId::LogsTitle: return L"Terminal y diagnóstico";
        case StringId::ClearBtn: return L"Limpiar";
        case StringId::MinimizeOnClose: return L"Minimizar a la bandeja al cerrar";
        case StringId::SettingsTitle: return L"CONFIGURACIÓN";
        case StringId::StartWithWindows: return L"Iniciar con Windows";
        case StringId::AutoStartService: return L"Iniciar el servicio automáticamente al abrir";
        case StringId::LowBatteryNotification: return L"Aviso de batería baja (<=15%)";
        case StringId::NintendoMode: return L"Modo Nintendo";
        case StringId::HairTrigger: return L"Hair Trigger (disparo instantáneo)";
        case StringId::PollingRateLabel: return L"Frecuencia máx. de actualización";
        case StringId::CurveLabel: return L"Curva del stick";
        case StringId::CurveLinear: return L"Lineal (1:1)";
        case StringId::CurveSmooth: return L"Apuntado suave";
        case StringId::CurveAggressive: return L"Agresiva";
        case StringId::DriverReadyFirstRun: return L"El controlador del gamepad (ViGEmBus) está detectado y listo. ¡A jugar!";
        case StringId::DriverMissing: return L"Se necesita el controlador del gamepad (ViGEmBus) para jugar.";
        case StringId::InstallDriverBtn: return L"Instalar controlador";
        case StringId::DriverInstalling: return L"Iniciando instalador... Acepta el aviso de Windows.";
        case StringId::DriverSuccess: return L"¡Controlador instalado correctamente! Listo para conectar.";
        case StringId::DriverFailed: return L"La instalación del controlador se canceló o no pudo completarse.";
        case StringId::DeadzoneLabel: return L"Deriva / zona muerta";
        case StringId::DeadzoneOff: return L"Apagada (0%)";
        case StringId::DeadzoneLow: return L"Baja (8%)";
        case StringId::DeadzoneNormal: return L"Normal (12%)";
        case StringId::DeadzoneHigh: return L"Alta (20%)";
        case StringId::SettingsBack: return L"Volver";
        case StringId::LiveInputTitle: return L"ENTRADA EN VIVO";
        case StringId::LowBatteryAlertTitle: return L"Batería baja en Ultimate2C";
        case StringId::LowBatteryAlertMsg: return L"La batería está al %d%%. Carga tu control.";
        case StringId::TrayOpen: return L"Abrir";
        case StringId::TrayExit: return L"Salir";
        case StringId::StatusSearching: return L"Buscando el control...";
        case StringId::StatusConnected: return L"Conectado y activo";
        case StringId::StatusStopped: return L"Servicio detenido";
        case StringId::NoDevice: return L"Sin dispositivo";
        case StringId::Idle: return L"Inactivo";
        case StringId::WaitingReconnect: return L"Esperando el control...";
        case StringId::LogAppReady: return L"Aplicación lista.";
        case StringId::LogServicesStarting: return L"Iniciando los servicios del control y de la batería...";
        case StringId::LogServicesStopping: return L"Deteniendo los servicios...";
        case StringId::LogMapperStart: return L"Servicio del control iniciado.";
        case StringId::LogMapperDisconnected: return L"Control desconectado. Buscando...";
        case StringId::LogBatteryActive: return L"Monitor de batería activo.";
        case StringId::HideInstallTitle: return L"Instalar controlador de ocultación";
        case StringId::HideInstallPrompt: return L"Para ocultar el control real se necesita el controlador gratuito HidHide (unos 8 MB, descargado desde su página oficial de versiones). ¿Descargarlo e instalarlo ahora?";
        case StringId::LogHideActive: return L"Control real oculto para los juegos.";
        case StringId::LogHideAlready: return L"El control real ya estaba oculto para los juegos.";
        case StringId::LogHideMissing: return L"El controlador de ocultación (HidHide) no está instalado. Los juegos pueden ver dos controles.";
        case StringId::LogHideNoDevice: return L"No se encontró el control real para ocultarlo.";
        case StringId::LogHideCustomSetup: return L"Ocultación omitida: tu configuración actual de HidHide usa un diseño personalizado y no se modificó.";
        case StringId::LogHideFailed: return L"No se pudo ocultar el control real. Los juegos pueden ver dos controles.";
        case StringId::LogHideRestored: return L"El control real vuelve a ser visible para los juegos.";
        case StringId::LogHideSuspended: return L"El control está conectado pero no se pudo acceder a él mientras estaba oculto. La ocultación se pausa durante un minuto.";
        case StringId::LogHideNeedsReconnect: return L"El control ya estaba conectado. Apágalo y vuelve a encenderlo una vez para que los navegadores y juegos vean un solo control.";
        case StringId::HideInstallBalloon: return L"Instala el controlador HidHide para que los juegos vean un solo control. Haz clic aquí para instalarlo.";
        case StringId::DisconnectOnExit: return L"Desconectar el control al cerrar la aplicación (pide permiso)";
        case StringId::LogVirtualPadPlayer: return L"El gamepad virtual es el jugador %d.";
        case StringId::LogHideRecovered: return L"Se restauró la visibilidad del control que quedó de una sesión anterior.";
        case StringId::LogHideRepaired: return L"Otro programa cambió la ocultación y se restableció. Si los juegos siguen viendo dos controles, apaga y vuelve a encender el control.";
        case StringId::LogHideInstalling: return L"Descargando controlador de ocultación... Acepta el aviso de Windows cuando aparezca.";
        case StringId::LogHideInstalled: return L"Controlador de ocultación instalado.";
        case StringId::LogHideInstallFailed: return L"No se pudo descargar el controlador de ocultación. Comprueba tu conexión a Internet.";
        case StringId::LogHideInstallUnverified: return L"No se pudo verificar el archivo descargado y no se ejecutó.";
        case StringId::LogHideInstallCancelled: return L"Se canceló la instalación del controlador de ocultación.";
        case StringId::LogHideInstallRestart: return L"Controlador de ocultación instalado. Reinicia Windows para terminar y luego inicia el servicio de nuevo.";
        case StringId::BatteryLowStatus: return L"Batería baja - Carga el control";
        case StringId::BatteryModerateStatus: return L"Inalámbrico - Nivel medio";
        case StringId::BatteryHealthyStatus: return L"Inalámbrico - Buen nivel";
        case StringId::DriverDownloadingShort: return L"Descargando %d%%";
        case StringId::LogViGEmUnavailable: return L"No se pudo conectar con el controlador del gamepad (ViGEmBus). Intenta reinstalarlo.";
        case StringId::LogInputSystemFailed: return L"No se pudo iniciar el sistema de entrada del control.";
        case StringId::LogControllerConnected: return L"%ls conectado.";
        case StringId::LogBatteryLevel: return L"%ls Batería: %d%%";
        default: return L"";
    }
}

} // namespace

LogLevel LevelOf(StringId id) {
    switch (id) {
        // Something worked.
        case StringId::LogControllerConnected:
        case StringId::LogHideActive:
        case StringId::LogHideAlready:
        case StringId::LogHideRestored:
        case StringId::LogHideRecovered:
        case StringId::LogHideInstalled:
        case StringId::DriverSuccess:
        case StringId::DriverReadyFirstRun:
            return LogLevel::Good;
        // Something failed or went away.
        case StringId::LogViGEmUnavailable:
        case StringId::LogInputSystemFailed:
        case StringId::LogMapperDisconnected:
        case StringId::LogHideFailed:
        case StringId::LogHideNoDevice:
        case StringId::LogHideInstallFailed:
        case StringId::LogHideInstallUnverified:
        case StringId::DriverFailed:
            return LogLevel::Bad;
        // Needs the user's attention or action.
        case StringId::LogHideMissing:
        case StringId::LogHideNeedsReconnect:
        case StringId::LogHideSuspended:
        case StringId::LogHideRepaired:
        case StringId::LogHideCustomSetup:
        case StringId::LogHideInstallRestart:
        case StringId::DriverMissing:
            return LogLevel::Critical;
        default:
            return LogLevel::Info;
    }
}

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
