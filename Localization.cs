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
        public string AppTitle => IsEnglish ? "8BitDo Controller Fixer" : "8BitDo Kontrolcü Düzeltici";
        public string ConnectionStatusTitle => IsEnglish ? "CONNECTION STATUS" : "BAĞLANTI DURUMU";
        public string BatteryLevelTitle => IsEnglish ? "BATTERY LEVEL" : "PİL SEVİYESİ";
        public string StartServiceBtn => IsEnglish ? "START SERVICE" : "BAŞLAT";
        public string StopServiceBtn => IsEnglish ? "STOP SERVICE" : "DURDUR";
        public string FooterText => "v0.0.1 • github.com/bezelye404";
        
        // Dynamic Texts (Used in Code-Behind)
        public string SearchingDInput => IsEnglish ? "Waiting for D-Input Device..." : "D-Input Cihazı Bekleniyor...";
        public string NoDevice => IsEnglish ? "No Device Connected" : "Cihaz Bağlı Değil";
        public string Scanning => IsEnglish ? "Scanning..." : "Taranıyor...";
        public string Connected => IsEnglish ? "Connection Established" : "Bağlantı Sağlandı";
        public string Stopped => IsEnglish ? "Stopped" : "Durduruldu";
        public string Idle => IsEnglish ? "Idle" : "Boşta";

        // Log messages
        public string LogAppInit => IsEnglish ? "Application Initialized." : "Uygulama başlatıldı.";
        public string LogServicesStarting => IsEnglish ? "Starting Services..." : "Servisler başlatılıyor...";
        public string LogServicesStopping => IsEnglish ? "Stopping Services..." : "Servisler durduruluyor...";
        
        public string LogMapperStart => IsEnglish ? "Bluetooth (DInput) -> Virtual Xbox 360 Remapper Started" : "Bluetooth (DInput) -> Sanal Xbox 360 Kontrolcüsü Başlatıldı";
        public string LogMapperNotFound => IsEnglish ? "ERROR: DInput gamepad/joystick not found!" : "HATA: DInput gamepad/joystick bulunamadı!";
        public string MapperNotFoundStatus => IsEnglish ? "Not Found" : "Bulunamadı";
        public string LogMapperSource(string name) => IsEnglish ? $"Source Device: {name}" : $"Kaynak Cihaz: {name}";
        public string MapperConnectedStatus => IsEnglish ? "Connected" : "Bağlandı";
        public string LogMapperReady => IsEnglish ? "Virtual Xbox Controller Connected. Ready!" : "Sanal Xbox Kontrolcüsü Bağlandı. Hazır!";
        public string LogMapperError(string ex) => IsEnglish ? $"Error or disconnected: {ex}" : $"Hata veya bağlantı koptu: {ex}";
        public string MapperDisconnectedStatus => IsEnglish ? "Disconnected" : "Bağlantı Koptu";

        public string LogBatteryStart(int init, int interval) => IsEnglish ? $"BLE Battery Monitor Started (Initial: {init}s, Interval: {interval}s)" : $"BLE Batarya Monitörü Başlatıldı (İlk Gecikme: {init}s, Aralık: {interval}s)";
        public string LogBatteryFatal(string ex) => IsEnglish ? $"[BLE Monitor Fatal Error]: {ex}" : $"[BLE Monitör Kritik Hata]: {ex}";
        public string LogBatteryLevel(string name, int level) => IsEnglish ? $"{name} Battery: {level}%" : $"{name} Pil: %{level}";
        public string LogBatteryScanError(string ex) => IsEnglish ? $"[BLE Scan Error]: {ex}" : $"[BLE Tarama Hatası]: {ex}";

        public event PropertyChangedEventHandler? PropertyChanged;
        protected void OnPropertyChanged([CallerMemberName] string? name = null)
        {
            PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(name));
        }
    }
}
