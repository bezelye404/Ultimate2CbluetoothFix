using System;
using System.Threading;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Threading;

namespace BitDoFixer
{
    public partial class MainWindow : Window
    {
        private CancellationTokenSource? _cts;
        private bool _isRunning = false;
        private RemapperStatus? _lastRemapperStatus;

        public MainWindow()
        {
            InitializeComponent();
            Log(Localization.Instance.LogAppInit);
        }

        private async void BtnStart_Click(object sender, RoutedEventArgs e)
        {
            StartService();
        }

        private void BtnStop_Click(object sender, RoutedEventArgs e)
        {
            StopService();
        }

        private void StartService()
        {
            if (_isRunning) return;

            _cts = new CancellationTokenSource();
            _isRunning = true;
            UpdateUiState(true);

            var loc = Localization.Instance;
            Log(loc.LogServicesStarting);

            _lastRemapperStatus = null;
            ApplyRemapperStatus(null);

            var hwnd = new System.Windows.Interop.WindowInteropHelper(this).Handle;
            var token = _cts.Token;

            _ = Task.Run(() => BluetoothRemapper.RunAsync(hwnd, token,
                logCallback: (msg) => Log($"[MAPPER] {msg}"),
                statusCallback: (status) => Dispatcher.Invoke(() => {
                    _lastRemapperStatus = status;
                    ApplyRemapperStatus(status);
                })
            ));

            _ = Task.Run(() => BluetoothBatteryMonitor.RunAsync(3, 300, token,
                logCallback: (msg) => Log($"[BATTERY] {msg}"),
                batteryCallback: (devName, level) => Dispatcher.Invoke(() => UpdateBattery(devName, level))
            ));
        }

        private void StopService()
        {
            if (!_isRunning) return;

            var loc = Localization.Instance;
            Log(loc.LogServicesStopping);
            _cts?.Cancel();
            _cts = null;
            _isRunning = false;
            _lastRemapperStatus = null;
            UpdateUiState(false);

            TxtRemapperStatus.Text = loc.Stopped;
            TxtDeviceInfo.Text = loc.Idle;
            BatteryProgress.Value = 0;
            TxtBatteryLevel.Text = "--%";
            TxtBatteryDevice.Text = loc.NoDevice;
        }

        private void UpdateUiState(bool running)
        {
            BtnStart.IsEnabled = !running;
            BtnStop.IsEnabled = running;
        }

        private void UpdateBattery(string deviceName, int level)
        {
            TxtBatteryLevel.Text = $"{level}%";
            BatteryProgress.Value = level;
            TxtBatteryDevice.Text = deviceName;
        }

        private void BtnLang_Click(object sender, RoutedEventArgs e)
        {
            var loc = Localization.Instance;
            loc.IsEnglish = !loc.IsEnglish;
            BtnLang.Content = loc.IsEnglish ? "TR" : "EN";

            if (!_isRunning)
            {
                TxtRemapperStatus.Text = loc.Stopped;
                TxtDeviceInfo.Text = loc.Idle;
                TxtBatteryDevice.Text = loc.NoDevice;
            }
            else
            {
                ApplyRemapperStatus(_lastRemapperStatus);
            }
        }

        private void ApplyRemapperStatus(RemapperStatus? status)
        {
            var loc = Localization.Instance;
            switch (status)
            {
                case RemapperStatus.Connected:
                    TxtRemapperStatus.Text = loc.MapperConnectedStatus;
                    TxtDeviceInfo.Text = loc.Connected;
                    break;
                case RemapperStatus.NotFound:
                    TxtRemapperStatus.Text = loc.MapperNotFoundStatus;
                    TxtDeviceInfo.Text = loc.SearchingDInput;
                    break;
                case RemapperStatus.Disconnected:
                    TxtRemapperStatus.Text = loc.MapperDisconnectedStatus;
                    TxtDeviceInfo.Text = loc.SearchingDInput;
                    break;
                default:
                    TxtRemapperStatus.Text = loc.Scanning;
                    TxtDeviceInfo.Text = loc.SearchingDInput;
                    break;
            }
        }

        private void Log(string message)
        {
            Dispatcher.Invoke(() =>
            {
                string time = DateTime.Now.ToString("HH:mm:ss");
                TxtLogs.AppendText($"[{time}] {message}\n");
                LogScroller.ScrollToBottom();
            });
        }

        protected override void OnClosed(EventArgs e)
        {
            StopService();
            base.OnClosed(e);
            Application.Current.Shutdown();
        }
    }
}
