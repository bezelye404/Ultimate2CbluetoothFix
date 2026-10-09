# Ultimate2CFixer

<div align="center">

![C++20](https://img.shields.io/badge/C%2B%2B-20-00599C?logo=c%2B%2B&logoColor=white)
![Platform](https://img.shields.io/badge/Platform-Windows%2010%20%2F%2011%20(x64)-0078D6?logo=windows&logoColor=white)
![ViGEmBus](https://img.shields.io/badge/Driver-ViGEmBus-blueviolet)
![Polling Rate](https://img.shields.io/badge/Polling%20Rate-Up%20to%201000%20Hz-orange)
![License](https://img.shields.io/badge/License-MIT-success)

DirectInput-to-XInput remapper and companion utility for the 8BitDo Ultimate 2C controller in Bluetooth mode on Windows.

[Overview](#-overview) • [Features](#-features) • [Installation & Usage](#-installation--usage) • [Hardware Axis Mapping](#-hardware-axis-mapping) • [Building from Source](#-building-from-source) • [License](#-license)

</div>

---

## 📖 Overview

When connected via Bluetooth on Windows, the 8BitDo Ultimate 2C controller communicates via DirectInput rather than XInput. Additionally, the right analog stick is exposed on DirectInput `lZ` and `lRz` axes.

Because most modern Windows games expect an XInput device, Ultimate2CFixer reads the DirectInput state from the controller, maps the inputs accordingly, and feeds them into a virtual Xbox 360 controller using ViGEmBus.

---

## ⚙️ Features

* **DirectInput to XInput Translation:** Maps Left Stick (`lX`, `lY`), Right Stick (`lZ`, `lRz`), triggers, and buttons to a virtual Xbox 360 gamepad.
* **Configurable Update Rate:** Maximum update rate (the app wakes only when the controller reports) of 125 Hz (8 ms), 250 Hz (4 ms), 500 Hz (2 ms), and 1000 Hz (1 ms).
* **Stick Response Curves:** Linear (1:1), Smooth Aim (cubic S-curve), and Aggressive (square-root).
* **Analog Triggers:** The analog pull depth (0-255) reaches the game. The trigger's digital click is only used as a fallback when the controller reports no analog trigger axes, so it never forces the full value. Behaves the same as the Linux version.
* **Optional Hair Trigger:** User-configurable toggle that maps any trigger pull beyond the deadzone directly to full activation (255).
* **Single Instance:** Starting a second copy brings the first window forward instead of running twice.
* **Live Input Telemetry:** Visual indicators for analog sticks, ABXY layout, LB/RB bumpers, and LT/RT analog triggers.
* **Readable Console:** Important lines of the console are coloured: green for something that worked, red for something that failed or went away, yellow for something that needs your attention.
* **Original Controller Hidden:** While the application is open, games and Steam only see the virtual gamepad, never the physical controller as well (no double inputs). This uses the [HidHide](https://github.com/nefarius/HidHide) driver, which the application offers to download and install, and it is always on. The controller is hidden before it connects, stays hidden when the service is stopped and started, and is put back within half a second if another program removes the hiding. Closing the application gives the controller back.
* **Battery Status:** Reads battery percentage via Windows WinRT Bluetooth Low Energy (BLE) GATT service.
* **System Tray & Resource Trimming:** Flushes physical working set memory when minimized to the system tray.
* **Closing:** The X button closes the application; the `_` button hides it to the system tray (the settings can make X do that too). Optionally the application can disconnect the controller when it closes, so browsers and games that were already using it see it disappear and arrive again. Windows asks for permission for this, only when the application is closed (not when the service is stopped).
* **Settings Persistence:** Saves user preferences to the Windows Registry (`HKCU\Software\Ultimate2CFixer\Settings`).
* **Localization:** English, Turkish and Spanish interface. The language button cycles through them and the choice is remembered.

---

## 🚀 Installation & Usage

### 1. Requirements
* Windows 10 / 11 (64-bit)
* [ViGEmBus](https://github.com/nefarius/ViGEmBus) driver (the application prompts to install this if not detected)
* [HidHide](https://github.com/nefarius/HidHide) driver to hide the original controller from games (the application offers to install it)

### 2. Quick Start
1. Download `Ultimate2CFixer.exe` from the [Releases](https://github.com/bezelye404/Ultimate2CbluetoothFix/releases) page.
2. Pair the 8BitDo Ultimate 2C controller via Windows Bluetooth settings.
3. Open `Ultimate2CFixer.exe` and click **Start Service**.

---

> [!NOTE]
> A program that was already using the controller when it got hidden (a browser tab, Steam) keeps it, and neither hiding nor un-hiding tells such a program that anything changed. If you open the application while the controller is already on, turn the controller off and on once (the application reminds you), and reload the browser page. Starting the application with Windows, before the controller connects, avoids this completely.

---

## 🎮 Hardware Axis Mapping

DirectInput `DIJOYSTATE2` axis layout for the 8BitDo Ultimate 2C in Bluetooth mode:

```
┌────────────────────────────────────────────────────────┐
│               8BitDo Ultimate 2C (Bluetooth)           │
├──────────────────────┬─────────────────────────────────┤
│ DirectInput Field    │ Gamepad Control                 │
├──────────────────────┼─────────────────────────────────┤
│ state.lX             │ Left Stick (Horizontal)         │
│ state.lY             │ Left Stick (Vertical)           │
│ state.lZ             │ Right Stick (Horizontal)        │
│ state.lRz            │ Right Stick (Vertical)          │
│ Brake axis (*)       │ Left Trigger (LT) Analog Depth  │
│ Accelerator axis (*) │ Right Trigger (RT) Analog Depth │
│ state.rgbButtons[6]  │ Left Bumper (LB)                │
│ state.rgbButtons[7]  │ Right Bumper (RB)               │
│ state.rgbButtons[8]  │ Left Trigger (LT) Digital Click │
│ state.rgbButtons[9]  │ Right Trigger (RT) Digital Click│
└──────────────────────┴─────────────────────────────────┘
```

> [!NOTE]
> The Right Stick is mapped to `lZ` and `lRz`, not `lRx`/`lRy`.

> [!NOTE]
> (*) The analog triggers are the HID usages Brake and Accelerator. DirectInput exposes them as extra axes that the standard `DIJOYSTATE2` layout has no slot for, so the Windows version reads them through a custom DirectInput data format that selects every control by its HID usage. The digital click is only a fallback and never forces the full value.

---

## 🔨 Building from Source

### Prerequisites
* Windows 10 / 11 (64-bit)
* Visual Studio 2022 with C++ desktop workload
* CMake 3.20 or newer

### Build Instructions
```powershell
# Clone the repository
git clone https://github.com/bezelye404/Ultimate2CbluetoothFix.git
cd Ultimate2CbluetoothFix

# Configure
cmake -B build -S .

# Build Release executable
cmake --build build --config Release
```

The compiled binary will be placed at:
```
build/Release/Ultimate2CFixer.exe
```

---

## 📝 License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

---

## 🙏 Acknowledgments

* [ViGEmBus](https://github.com/nefarius/ViGEmBus) by Benjamin Höglinger-Stelzer (Nefarius).
* [HidHide](https://github.com/nefarius/HidHide) by Eric Korff de Gidts and Benjamin Höglinger-Stelzer (Nefarius).

Third-party license texts are listed in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
