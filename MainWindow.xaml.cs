using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Threading;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Interop;
using System.Windows.Media;

namespace BitDoFixer
{
    public partial class MainWindow : Window
    {
        private const int MaxLogLines = 100;
        private readonly Queue<string> _logLines = new();

        private CancellationTokenSource? _cts;
        private bool _isRunning = false;
        private RemapperStatus _lastRemapperStatus = RemapperStatus.Stopped;
        private string? _activeDeviceName = null;
        private bool _isLogsVisible = true;

        // Tray Icon Win32 API
        private const int WM_USER = 0x0400;
        private const int WM_TRAYICON = WM_USER + 1;
        private const int WM_LBUTTONUP = 0x0202;
        private const int WM_LBUTTONDBLCLK = 0x0203;
        private const int WM_RBUTTONUP = 0x0205;
        private const int NIM_ADD = 0x00000000;
        private const int NIM_DELETE = 0x00000002;
        private const int NIF_MESSAGE = 0x00000001;
        private const int NIF_ICON = 0x00000002;
        private const int NIF_TIP = 0x00000004;

        [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
        private struct NOTIFYICONDATA
        {
            public int cbSize;
            public IntPtr hWnd;
            public int uID;
            public int uFlags;
            public int uCallbackMessage;
            public IntPtr hIcon;
            [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 128)]
            public string szTip;
            public int dwState;
            public int dwStateMask;
            [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 256)]
            public string szInfo;
            public int uTimeoutOrVersion;
            [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 64)]
            public string szInfoTitle;
            public int dwInfoFlags;
        }

        [DllImport("shell32.dll", CharSet = CharSet.Unicode)]
        private static extern bool Shell_NotifyIcon(int dwMessage, ref NOTIFYICONDATA lpData);

        [DllImport("kernel32.dll")]
        private static extern bool SetProcessWorkingSetSize(IntPtr proc, IntPtr min, IntPtr max);

        private NOTIFYICONDATA _trayData;
        private bool _trayCreated = false;
        private HwndSource? _hwndSource;

        public MainWindow()
        {
            InitializeComponent();
            Log(Localization.Instance.LogAppInit);
            UpdateStatusDisplay(RemapperStatus.Stopped, null);
        }

        protected override void OnSourceInitialized(EventArgs e)
        {
            base.OnSourceInitialized(e);
            var helper = new WindowInteropHelper(this);
            _hwndSource = HwndSource.FromHwnd(helper.Handle);
            _hwndSource?.AddHook(WndProc);
            SetupTrayIcon(helper.Handle);
        }

        private void SetupTrayIcon(IntPtr hwnd)
        {
            try
            {
                var iconHandle = (Icon as System.Windows.Media.Imaging.BitmapFrame)?.Decoder?.Frames[0];
                IntPtr hIcon = IntPtr.Zero;

                using (var iconStream = Application.GetResourceStream(new Uri("pack://application:,,,/Assets/logo.ico"))?.Stream)
                {
                    if (iconStream != null)
                    {
                        using var ico = new System.Drawing.Icon(iconStream);
                        hIcon = ico.Handle;
                    }
                }

                _trayData = new NOTIFYICONDATA
                {
                    cbSize = Marshal.SizeOf(typeof(NOTIFYICONDATA)),
                    hWnd = hwnd,
                    uID = 1001,
                    uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP,
                    uCallbackMessage = WM_TRAYICON,
                    hIcon = hIcon,
                    szTip = "8BitDo Ultimate 2C Fixer"
                };

                _trayCreated = Shell_NotifyIcon(NIM_ADD, ref _trayData);
            }
            catch
            {
                // Tray initialization fallback
            }
        }

        private IntPtr WndProc(IntPtr hwnd, int msg, IntPtr wParam, IntPtr lParam, ref bool handled)
        {
            if (msg == WM_TRAYICON)
            {
                int l = lParam.ToInt32();
                if (l == WM_LBUTTONDBLCLK || l == WM_LBUTTONUP)
                {
                    RestoreFromTray();
                    handled = true;
                }
                else if (l == WM_RBUTTONUP)
                {
                    ShowTrayMenu();
                    handled = true;
                }
            }
            return IntPtr.Zero;
        }

        private void ShowTrayMenu()
        {
            var loc = Localization.Instance;
            var menu = new ContextMenu();

            var openItem = new MenuItem { Header = loc.TrayOpen };
            openItem.Click += (s, e) => RestoreFromTray();
            menu.Items.Add(openItem);

            menu.Items.Add(new Separator());

            var exitItem = new MenuItem { Header = loc.TrayExit };
            exitItem.Click += (s, e) => Close();
            menu.Items.Add(exitItem);

            menu.IsOpen = true;
        }

        private void RestoreFromTray()
        {
            Show();
            WindowState = WindowState.Normal;
            Activate();
        }

        private void BtnMinimizeTray_Click(object sender, RoutedEventArgs e)
        {
            Hide();
            TrimWorkingSet();
        }

        private static void TrimWorkingSet()
        {
            try
            {
                GC.Collect(2, GCCollectionMode.Forced, true);
                GC.WaitForPendingFinalizers();
                SetProcessWorkingSetSize(System.Diagnostics.Process.GetCurrentProcess().Handle, (IntPtr)(-1), (IntPtr)(-1));
            }
            catch { }
        }

        private void BtnStart_Click(object sender, RoutedEventArgs e)
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
            UpdateUiButtons(true);

            var loc = Localization.Instance;
            Log(loc.LogServicesStarting);

            UpdateStatusDisplay(RemapperStatus.Searching, null);

            var hwnd = new WindowInteropHelper(this).Handle;
            var token = _cts.Token;

            _ = Task.Run(() => BluetoothRemapper.RunAsync(
                hwnd,
                token,
                logCallback: (msg) => Log(msg),
                statusCallback: (status, devName) => Dispatcher.Invoke(() => {
                    _lastRemapperStatus = status;
                    _activeDeviceName = devName;
                    UpdateStatusDisplay(status, devName);
                })
            ));

            _ = Task.Run(() => BluetoothBatteryMonitor.RunAsync(
                initialDelaySeconds: 2,
                intervalSeconds: 300,
                token: token,
                logCallback: (msg) => Log(msg),
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
            _activeDeviceName = null;
            _lastRemapperStatus = RemapperStatus.Stopped;
            UpdateUiButtons(false);

            UpdateStatusDisplay(RemapperStatus.Stopped, null);
            ProgressBattery.Value = 0;
            TxtBatteryPercent.Text = "--%";
            TxtBatteryDevice.Text = loc.NoDevice;
            ProgressBattery.Foreground = (Brush)FindResource("TextSecondary");

            TrimWorkingSet();
        }

        private void UpdateUiButtons(bool running)
        {
            BtnStart.IsEnabled = !running;
            BtnStop.IsEnabled = running;
        }

        private void UpdateBattery(string deviceName, int level)
        {
            TxtBatteryPercent.Text = $"{level}%";
            ProgressBattery.Value = Math.Clamp(level, 0, 100);
            TxtBatteryDevice.Text = deviceName;

            if (level <= 20)
                ProgressBattery.Foreground = (Brush)FindResource("StatusDisconnected");
            else if (level <= 50)
                ProgressBattery.Foreground = (Brush)FindResource("StatusSearching");
            else
                ProgressBattery.Foreground = (Brush)FindResource("StatusConnected");
        }

        private void UpdateStatusDisplay(RemapperStatus status, string? deviceName)
        {
            var loc = Localization.Instance;

            switch (status)
            {
                case RemapperStatus.Connected:
                    TxtDeviceName.Text = !string.IsNullOrEmpty(deviceName) ? deviceName : "8BitDo Controller";
                    TxtStatusDetail.Text = loc.Connected;
                    DotStatus.Fill = (Brush)FindResource("StatusConnected");
                    break;

                case RemapperStatus.Searching:
                    TxtDeviceName.Text = loc.SearchingDInput;
                    TxtStatusDetail.Text = loc.Scanning;
                    DotStatus.Fill = (Brush)FindResource("StatusSearching");
                    break;

                case RemapperStatus.Disconnected:
                    TxtDeviceName.Text = loc.NoDevice;
                    TxtStatusDetail.Text = loc.WaitingReconnect;
                    DotStatus.Fill = (Brush)FindResource("StatusDisconnected");
                    break;

                case RemapperStatus.Stopped:
                default:
                    TxtDeviceName.Text = loc.NoDevice;
                    TxtStatusDetail.Text = loc.Stopped;
                    DotStatus.Fill = (Brush)FindResource("StatusIdle");
                    break;
            }
        }

        private void BtnToggleLogs_Click(object sender, RoutedEventArgs e)
        {
            _isLogsVisible = !_isLogsVisible;
            PanelLogs.Visibility = _isLogsVisible ? Visibility.Visible : Visibility.Collapsed;
        }

        private void BtnClearLogs_Click(object sender, RoutedEventArgs e)
        {
            _logLines.Clear();
            TxtLogs.Clear();
        }

        private void BtnLang_Click(object sender, RoutedEventArgs e)
        {
            var loc = Localization.Instance;
            loc.IsEnglish = !loc.IsEnglish;
            BtnLang.Content = loc.IsEnglish ? "TR" : "EN";

            UpdateStatusDisplay(_lastRemapperStatus, _activeDeviceName);
            if (!_isRunning)
            {
                TxtBatteryDevice.Text = loc.NoDevice;
            }
        }

        private void Log(string message)
        {
            Dispatcher.Invoke(() =>
            {
                string time = DateTime.Now.ToString("HH:mm:ss");
                string line = $"[{time}] {message}";

                _logLines.Enqueue(line);
                while (_logLines.Count > MaxLogLines)
                {
                    _logLines.Dequeue();
                }

                TxtLogs.Text = string.Join(Environment.NewLine, _logLines);
                LogScroller.ScrollToBottom();
            });
        }

        protected override void OnClosed(EventArgs e)
        {
            if (_trayCreated)
            {
                Shell_NotifyIcon(NIM_DELETE, ref _trayData);
            }
            StopService();
            base.OnClosed(e);
            Application.Current.Shutdown();
        }
    }
}
