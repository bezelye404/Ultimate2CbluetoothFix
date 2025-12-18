# 8BitDo Fixer

A Windows application that remaps 8BitDo controllers (DirectInput mode) to a virtual Xbox 360 controller, with battery monitoring support.

![.NET](https://img.shields.io/badge/.NET-10.0-512BD4?logo=dotnet)
![Windows](https://img.shields.io/badge/Windows-10+-0078D6?logo=windows)
![License](https://img.shields.io/badge/License-MIT-green)

## ✨ Features

- **Controller Remapping**: Converts 8BitDo DirectInput to Xbox 360 controller
- **Battery Monitoring**: Displays battery level via Bluetooth GATT
- **Modern UI**: Dark-themed Material Design interface
- **Single EXE**: No installation required

## 📋 Requirements

### For Users (EXE)
- Windows 10 (Build 19041 or later)
- [ViGEmBus Driver](https://github.com/nefarius/ViGEmBus/releases) - **Required**

### For Developers (Source)
- .NET 10 SDK
- Visual Studio 2022 or VS Code

## 🚀 Installation

1. Download `8bitdofixer.exe` from [Releases](../../releases)
2. Install [ViGEmBus Driver](https://github.com/nefarius/ViGEmBus/releases)
3. Run the EXE - no installation needed!

## 🎮 Usage

1. Connect your 8BitDo controller via Bluetooth (DirectInput mode)
2. Launch **8BitDo Fixer**
3. The app will automatically:
   - Detect your controller
   - Create a virtual Xbox 360 controller
   - Start remapping inputs
4. Battery level updates every 5 minutes

## 🔧 Building from Source

```bash
# Clone the repository
git clone https://github.com/bezelye404/8bitdo-fixer.git
cd 8bitdo-fixer

# Build
dotnet build

# Run
dotnet run

# Publish (single file)
dotnet publish -c Release -o ./publish
```

## 📁 Project Structure

```
8bitdofixer/
├── App.xaml                    # Application definition
├── MainWindow.xaml             # UI layout
├── MainWindow.xaml.cs          # UI logic
├── BluetoothRemapper.cs        # Controller remapping
├── BluetoothBatteryMonitor.cs  # Battery monitoring
├── Program.cs                  # Entry point
└── 8bitdofixer.csproj          # Project file
```

## 🔌 Dependencies

| Package | Purpose |
|---------|---------|
| [MaterialDesignThemes](https://github.com/MaterialDesignInXAML/MaterialDesignInXamlToolkit) | UI Styling |
| [ViGEm.Client](https://github.com/nefarius/ViGEm.Client) | Virtual Controller |
| [SharpDX.DirectInput](http://sharpdx.org/) | Controller Input |

## ⚠️ Known Limitations

- Triggers work as digital buttons (hardware limitation in DirectInput mode)
- Battery monitoring only works with 8BitDo controllers that support Bluetooth GATT

## 📝 License

MIT License - feel free to use and modify.

## 🙏 Credits

- [ViGEmBus](https://github.com/nefarius/ViGEmBus) by Nefarius
- [MaterialDesignInXAML](https://github.com/MaterialDesignInXAML/MaterialDesignInXamlToolkit)
