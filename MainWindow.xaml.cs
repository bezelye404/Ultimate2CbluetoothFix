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

        public MainWindow()
        {
            InitializeComponent();
            Log("Application Initialized.");
            StartService();
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

            Log("Starting Services...");

            var token = _cts.Token;

            _ = Task.Run(() => BluetoothRemapper.RunAsync(token, 
                logCallback: (msg) => Log($"[MAPPER] {msg}"),
                statusCallback: (status) => Dispatcher.Invoke(() => {
                    TxtRemapperStatus.Text = status;
                    if(status.Contains("Found")) TxtDeviceInfo.Text = "Connected";
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

            Log("Stopping Services...");
            _cts?.Cancel();
            _cts = null;
            _isRunning = false;
            UpdateUiState(false);
            
            TxtRemapperStatus.Text = "Stopped";
            TxtDeviceInfo.Text = "Idle";
        }

        private void UpdateUiState(bool running)
        {
            BtnStart.IsEnabled = !running;
            BtnStop.IsEnabled = running;
        }

        private void UpdateBattery(string deviceName, int level)
        {
            TxtBatteryLevel.Text = $"{level}%";
            TxtBatteryDevice.Text = deviceName;
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
