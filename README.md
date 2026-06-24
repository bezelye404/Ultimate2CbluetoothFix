# 8BitDo Ultimate 2C Bluetooth Fixer

An elegant, modern Windows application that remaps 8BitDo Ultimate 2C controllers (connected via Bluetooth) to a virtual XInput (Xbox 360) controller. It also supports battery monitoring and force feedback (vibration).

![.NET](https://img.shields.io/badge/.NET-10.0-512BD4?logo=dotnet)
![Windows](https://img.shields.io/badge/Windows-10+-0078D6?logo=windows)
![License](https://img.shields.io/badge/License-MIT-green)

---

## ✨ Features

- 🎮 **Controller Remapping**: Flawlessly maps the 8BitDo Ultimate 2C Bluetooth DirectInput layout to a virtual Xbox 360 controller.
- 📳 **Rumble / Force Feedback Support**: Direct mapping of XInput motor vibration commands back to the controller via DirectInput force feedback actuators.
- 🔋 **Battery Monitoring**: Reads and displays the real-time battery level of the controller using Bluetooth GATT services.
- 🎨 **Modern UI**: A fully overhauled, responsive, and clean user interface powered by Material Design, featuring dynamic status indicators.
- 🌐 **Bilingual (EN / TR)**: Full localization support with automatic language detection and switching.

---

## 📋 Requirements

### For Users (Run/Release)
* Windows 10 (Build 19041 or later)
* [ViGEmBus Driver](https://github.com/nefarius/ViGEmBus/releases) — **Required** (This is the virtual controller driver that emulates the Xbox 360 pad).

### For Developers (Build)
* .NET 10 SDK

---

## 🚀 Installation & Usage

1. Download the latest release (`8bitdofixer.exe`) from the [Releases](../../releases) section.
2. Install the [ViGEmBus Driver](https://github.com/nefarius/ViGEmBus/releases) if you haven't already.
3. Turn on your controller in **Bluetooth mode** (Bluetooth/Android mode) and pair it with Windows.
4. Launch `8bitdofixer.exe`.
5. Click **Start** to run the remapper service. The app will:
   * Detect the connected controller.
   * Emulate a virtual Xbox 360 controller.
   * Start remapping inputs and routing rumble signals.
   * Periodically check and display the battery level.

---

## 🔧 Building from Source

To build or run the application from source:

```bash
# Clone the repository
git clone https://github.com/bezelye404/Ultimate2CbluetoothFix.git
cd Ultimate2CbluetoothFix

# Run the app
dotnet run

# Publish a single-file, self-contained Release build
dotnet publish -c Release
```

The published build will be located under `bin/Release/net10.0-windows10.0.19041.0/win-x64/publish/`.

---

## 🔌 Dependencies

The project leverages the following main libraries:

| Package | Version | Purpose |
|---------|---------|---------|
| [MaterialDesignThemes](https://github.com/MaterialDesignInXAML/MaterialDesignInXamlToolkit) | `5.3.0` | Modern UI Theme & Icon pack |
| [Nefarius.ViGEm.Client](https://github.com/nefarius/ViGEm.Client) | `1.x` | Virtual Xbox 360 emulation |
| [SharpDX.DirectInput](http://sharpdx.org/) | `4.2.0` | Fetching DirectInput states and sending vibration commands |

---

## ⚠️ Limitations

* **Trigger Axes:** The triggers function as digital buttons. This is a hardware/firmware constraint of the 8BitDo Ultimate 2C controller when running in Android/Bluetooth (DirectInput) mode, as it only reports them as button presses rather than analog axes.

---

## 📝 License

Distributed under the **MIT License**. See `LICENSE` for more information.

---

## 🙏 Credits

* [ViGEmBus](https://github.com/nefarius/ViGEmBus) by Nefarius
* [MaterialDesignInXAML](https://github.com/MaterialDesignInXAML/MaterialDesignInXamlToolkit)
* [SharpDX](https://github.com/sharpdx/SharpDX)
