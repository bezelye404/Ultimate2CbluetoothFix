using System.ComponentModel;
using System.Runtime.CompilerServices;

namespace BitDoFixer
{
    public class Localization : INotifyPropertyChanged
    {
        public static Localization Instance { get; } = new Localization();

        private bool _isEnglish = true;
        public bool IsEnglish
        {
            get => _isEnglish;
            set
            {
                if (_isEnglish != value)
                {
                    _isEnglish = value;
                    OnPropertyChanged(null); // Notify all properties
                }
            }
        }

        // XAML Bound Properties
        public string AppTitle => IsEnglish ? "8BitDo Ultimate 2C Fixer" : "8BitDo Ultimate 2C Düzeltici";
        public string ConnectionStatusTitle => IsEnglish ? "CONTROLLER STATUS" : "KONTROLCÜ DURUMU";
        public string BatteryLevelTitle => IsEnglish ? "BATTERY" : "PİL";
        public string InputModeTitle => IsEnglish ? "TRANSLATION" : "DÖNÜŞÜM";
        public string InputModeDesc => "DirectInput ➔ XInput (Xbox 360)";
        public string StartServiceBtn => IsEnglish ? "Start Service" : "Servisi Başlat";
        public string StopServiceBtn => IsEnglish ? "Stop Service" : "Servisi Durdur";
        public string MinimizeToTrayBtn => IsEnglish ? "Minimize to Tray" : "Tepsiye Küçült";
        public string ToggleLogsText => IsEnglish ? "Logs & Diagnostics" : "Günlükler ve Tanılama";
        public string ClearLogsBtn => IsEnglish ? "Clear" : "Temizle";
        public string FooterText => "v0.1.0 • github.com/bezelye404";
        public string TrayOpen => IsEnglish ? "Open" : "Aç";
        public string TrayExit => IsEnglish ? "Exit" : "Çıkış";
        
        // Dynamic Texts (Used in Code-Behind)
        public string SearchingDInput => IsEnglish ? "Searching for controller..." : "Kontrolcü aranıyor...";
        public string NoDevice => IsEnglish ? "No Device Detected" : "Cihaz Algılanmadı";
        public string Scanning => IsEnglish ? "Scanning..." : "Taranıyor...";
        public string Connected => IsEnglish ? "Connected & Active" : "Bağlı ve Aktif";
        public string Stopped => IsEnglish ? "Service Stopped" : "Servis Durduruldu";
        public string Idle => IsEnglish ? "Idle" : "Boşta";
        public string WaitingReconnect => IsEnglish ? "Waiting for controller..." : "Kolun açılması bekleniyor...";

        // Log messages
        public string LogAppInit => IsEnglish ? "Application ready." : "Uygulama hazır.";
        public string LogServicesStarting => IsEnglish ? "Starting services..." : "Servisler başlatılıyor...";
        public string LogServicesStopping => IsEnglish ? "Stopping services..." : "Servisler durduruluyor...";
        
        public string LogMapperStart => IsEnglish ? "Bluetooth (DInput) -> Virtual Xbox 360 Remapper started." : "Bluetooth (DInput) -> Sanal Xbox 360 Remapper başlatıldı.";
        public string LogMapperNotFound => IsEnglish ? "Controller not found. Entering auto-reconnect scan..." : "Kontrolcü bulunamadı. Otomatik arama moduna geçiliyor...";
        public string MapperNotFoundStatus => IsEnglish ? "Not Found" : "Bulunamadı";
        public string LogMapperSource(string name) => IsEnglish ? $"Connected to device: {name}" : $"Cihaza bağlanıldı: {name}";
        public string MapperConnectedStatus => IsEnglish ? "Connected" : "Bağlandı";
        public string LogMapperReady => IsEnglish ? "Virtual Xbox 360 controller active and ready." : "Sanal Xbox 360 kontrolcüsü aktif ve hazır.";
        public string LogMapperError(string ex) => IsEnglish ? $"Connection dropped: {ex}. Reconnecting..." : $"Bağlantı koptu: {ex}. Yeniden bağlanılıyor...";
        public string MapperDisconnectedStatus => IsEnglish ? "Disconnected" : "Bağlantı Koptu";

        public string LogBatteryStart(int init, int interval) => IsEnglish ? $"Battery monitor active (refresh: {interval}s)." : $"Pil monitörü aktif (yenileme: {interval}sn).";
        public string LogBatteryFatal(string ex) => IsEnglish ? $"Battery monitor error: {ex}" : $"Pil monitörü hatası: {ex}";
        public string LogBatteryLevel(string name, int level) => IsEnglish ? $"{name}: {level}%" : $"{name}: %{level}";
        public string LogBatteryScanError(string ex) => IsEnglish ? $"Battery scan warning: {ex}" : $"Pil tarama uyarısı: {ex}";

        public event PropertyChangedEventHandler? PropertyChanged;
        protected void OnPropertyChanged([CallerMemberName] string? name = null)
        {
            PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(name));
        }
    }
}
