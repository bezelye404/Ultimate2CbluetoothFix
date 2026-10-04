# Ultimate2CFixer

<div align="center">

![C++20](https://img.shields.io/badge/C%2B%2B-20-00599C?logo=c%2B%2B&logoColor=white)
![Platform](https://img.shields.io/badge/Platform-Windows%2010%20%2F%2011%20(x64)%20%7C%20Linux-0078D6?logo=windows&logoColor=white)
![Qt](https://img.shields.io/badge/Linux%20UI-Qt%206-41CD52?logo=qt&logoColor=white)
![ViGEmBus](https://img.shields.io/badge/Windows%20Driver-ViGEmBus-blueviolet)
![Polling Rate](https://img.shields.io/badge/Update%20Rate-Up%20to%201000%20Hz-orange)
![Languages](https://img.shields.io/badge/Languages-EN%20%7C%20TR%20%7C%20ES-informational)
![License](https://img.shields.io/badge/License-MIT-success)

8BitDo Ultimate 2C Bluetooth fix: turns the controller's Bluetooth input into a virtual Xbox 360 (XInput) gamepad on Windows and Linux.

[Overview](#-overview) • [Features](#-features) • [Windows](#-windows) • [Linux](#-linux) • [Hardware Mapping](#-hardware-mapping) • [Known Limitations](#-known-limitations) • [Building from Source](#-building-from-source) • [License](#-license)

</div>

---

## 📖 Overview

Over Bluetooth, the 8BitDo Ultimate 2C does not present itself as an Xbox controller. It shows up as a generic HID gamepad (USB vendor `0x2DC8`, product `0x301B`), with the right analog stick on unusual axes and the triggers split into an analog axis and a digital click. Many games and launchers only understand XInput, so they ignore the controller or read it wrongly.

Ultimate2CFixer reads that Bluetooth gamepad, applies a fixed mapping, and feeds the result into a virtual Xbox 360 controller. It runs as a small desktop app with a window, a system tray icon, live input display and battery level.

* **Windows:** reads DirectInput and feeds the virtual pad through the ViGEmBus driver.
* **Linux:** reads the kernel's evdev device and creates the virtual pad through `/dev/uinput`.

This tool is for **Bluetooth mode** only. The 2.4 GHz and wired modes are not what it was built for.

> [!NOTE]
> Designed for and tested on the Ultimate 2C. Other 8BitDo models are recognized, but correct operation is not guaranteed.

---

## ⚙️ Features

* **Controller to virtual Xbox 360 pad:** sticks, triggers, bumpers, face buttons, Back, Start, stick clicks and the D-pad.
* **Configurable update rate:** 125 Hz (8 ms), 250 Hz (4 ms), 500 Hz (2 ms) or 1000 Hz (1 ms).
* **Stick response curves:** Linear (1:1), Smooth Aim (smoothstep S-curve) and Aggressive (square root).
* **Dead zone presets:** Off, Low (8%), Normal (12%) and High (20%).
* **Hair Trigger:** optional; any trigger pull past a small threshold (about 2% of the travel) counts as full (255).
* **Nintendo Mode:** swaps A with B and X with Y.
* **Live input display:** sticks, ABXY layout, LB/RB and both trigger bars, drawn from the mapped values.
* **Battery level:** read from the controller over Bluetooth Low Energy.
* **System tray:** the window can hide to the tray; low battery warning at 15% or lower.
* **Languages:** English and Turkish on Windows; English, Turkish and Spanish on Linux (the Spanish text is a draft that still needs a native reader).

### Linux only

* **Working analog triggers:** the trigger depth reaches the game as an analog value (see [Known Limitations](#-known-limitations) for Windows).
* **Hide the real controller from other programs** (on by default): while connected, games only see the virtual gamepad, so inputs are not counted twice. It can be switched off in the settings.
* **Start when I log in:** writes an autostart entry; the app starts hidden in the tray.
* **Single instance:** starting a second copy brings the first window forward.
* **Missing component notices:** if a permission or a service is missing, the app says what is wrong in plain words and shows commands to copy. It never runs them.
* **Settings file:** plain text, editable by hand, and also editable in the window.

---

## 🪟 Windows

### Requirements
* Windows 10 / 11 (64-bit)
* [ViGEmBus](https://github.com/nefarius/ViGEmBus) driver (the application offers to install it if it is not detected)

### Quick Start
1. Download `Ultimate2CFixer.exe` from the [Releases](https://github.com/bezelye404/Ultimate2CbluetoothFix/releases) page.
2. Pair the 8BitDo Ultimate 2C via Windows Bluetooth settings.
3. Open `Ultimate2CFixer.exe` and click **Start Service**.

Settings are stored in the Windows Registry (`HKCU\Software\Ultimate2CFixer\Settings`).

---

## 🐧 Linux

> [!IMPORTANT]
> The Linux version is in development on the `linux-port` branch. There is no release and no distribution package yet; it has to be built from source. See [Status](#status-of-the-linux-version) for what has and has not been verified.

### Requirements
* A Linux kernel with `evdev` and `uinput` (`/dev/uinput` must exist and your user needs write access to it).
* Qt 6.4 or newer for the window (without Qt only the command line runner is built).
* BlueZ for the battery level (the app works without it, but shows no battery).
* A system tray: KDE Plasma has one. GNOME shows none by default and needs the AppIndicator extension; without a tray, closing the window quits the app.

### Run
```sh
cmake -B build -S linux && cmake --build build
build/app/ultimate2cfixer
```

The translations and the icon are looked up at run time in a `data` folder next to the program (the build folder has one) and in `share/ultimate2cfixer` of the XDG data folders. To install the programs and that data into a folder of your choice:

```sh
cmake --install build --prefix ~/.local
```

Useful options: `--minimized` (start hidden in the tray), `--demo` (fake controller, nothing is written), `--settings <file>`, `--language en|tr|es`. A command line runner without a window is built as `build/app/ultimate2cfixer-headless` (`--probe` only looks for the controller and the battery).

### Settings
`~/.config/ultimate2cfixer/settings.conf`, one `Key=Value` per line. Keys: `MinimizeOnClose`, `AutoStart`, `LowBatteryAlert`, `NintendoMode`, `HairTrigger`, `ExclusiveGrab`, `Deadzone` (0-3), `PollingRate` (0-3), `ResponseCurve` (0-2) and `Language` (`en`, `tr`, `es`; empty means the system language). Comments and unknown keys are kept when the app saves.

### Status of the Linux version
| Area | State |
|---|---|
| Mapping, curves, triggers, settings file, translations | Covered by automated tests that ran on every change |
| Detecting the controller and reading the battery | Confirmed on a real Ultimate 2C (CachyOS, BlueZ 5.87) |
| Window, settings screen, tray, dialogs, three languages | Checked on screenshots and by hand on KDE Plasma (Wayland) |
| Virtual gamepad, hiding the real controller, update rate limit, reconnect | Implemented and tested with fake devices; the full check with a real controller and real games is still open |
| RAM | About 25 to 29 MB (PSS) on KDE Plasma, the figure varies by a few MB between starts and may be higher on other desktops |
| Other distributions and desktops | Not tested (only CachyOS with KDE Plasma was available) |

---

## 🎮 Hardware Mapping

### Windows (DirectInput `DIJOYSTATE2`)

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
│ state.rglSlider[0]   │ Left Trigger (LT) Analog Depth  │
│ state.rglSlider[1]   │ Right Trigger (RT) Analog Depth │
│ state.rgbButtons[0]  │ A                               │
│ state.rgbButtons[1]  │ B                               │
│ state.rgbButtons[3]  │ X                               │
│ state.rgbButtons[4]  │ Y                               │
│ state.rgbButtons[6]  │ Left Bumper (LB)                │
│ state.rgbButtons[7]  │ Right Bumper (RB)               │
│ state.rgbButtons[8]  │ Left Trigger (LT) Digital Click │
│ state.rgbButtons[9]  │ Right Trigger (RT) Digital Click│
│ state.rgbButtons[10] │ Back                            │
│ state.rgbButtons[11] │ Start                           │
│ state.rgbButtons[13] │ Left Stick Click (L3)           │
│ state.rgbButtons[14] │ Right Stick Click (R3)          │
└──────────────────────┴─────────────────────────────────┘
```

Besides `rglSlider`, the Windows version also reads `lRx` and `lRy` for the triggers and uses whichever value is larger. Buttons 2, 5 and 12 are not used by the Windows version.

> [!NOTE]
> The Right Stick is mapped to `lZ` and `lRz`, not `lRx`/`lRy`.

### Linux (evdev, measured on the Bluetooth device `2dc8:301b`)

| evdev event | Control |
|---|---|
| `ABS_X`, `ABS_Y` (0 to 255) | Left stick |
| `ABS_Z`, `ABS_RZ` (0 to 255) | Right stick |
| `ABS_BRAKE`, `BTN_TL2` | Left trigger (analog depth, digital click) |
| `ABS_GAS`, `BTN_TR2` | Right trigger (analog depth, digital click) |
| `BTN_SOUTH`, `BTN_EAST`, `BTN_NORTH`, `BTN_WEST` | A, B, X, Y |
| `BTN_TL`, `BTN_TR` | LB, RB |
| `BTN_SELECT`, `BTN_START` | Back, Start |
| `BTN_THUMBL`, `BTN_THUMBR` | L3, R3 |
| `ABS_HAT0X`, `ABS_HAT0Y` | D-pad |
| `BTN_MODE`, `BTN_C`, `BTN_Z` | Home, back buttons (L4, R4): reported by the controller, not mapped yet |

The controller reports sticks and triggers with 256 steps (0 to 255). The mapping scales them to the 16-bit range the formulas use, so the output cannot be finer than those 256 steps.

---

## ⚠️ Known Limitations

* **Analog triggers do not work on Windows.** The digital click of a trigger fires early in the pull (about 14% of the travel was measured) and forces the full value, so the analog depth is never used. The Linux version ignores the click when the analog value is present.
* **ViGEmBus is no longer maintained.** Its repository is archived and marked as retired. The Windows version depends on it.
* **The back buttons (L4, R4) and Home are not mapped.** On Linux they are reported by the controller and ignored; on Windows buttons 2, 5 and 12 are ignored.
* **Rumble is not supported** over Bluetooth: the controller does not expose force feedback in this mode.
* **Update rate:** on Windows the loop sleeps for the set interval after each pass, so the real rate is at or below the setting (not measured). On Linux the setting is a maximum update rate: the controller only reports changes, and nothing is written while it is idle.
* **Linux:** the app needs write access to `/dev/uinput`, a tray host is needed to hide the window, only KDE Plasma was available for testing, Flatpak and Snap are not supported, and the commands shown in the missing component dialogs have not been tested on every distribution.
* The Spanish text is a draft; the Turkish text of the newer messages was written by the same author and has not been reviewed by a second reader.

---

## 🔨 Building from Source

### Windows
Prerequisites: Windows 10 / 11 (64-bit), Visual Studio 2022 with the C++ desktop workload, CMake 3.20 or newer.

```powershell
git clone https://github.com/bezelye404/Ultimate2CbluetoothFix.git
cd Ultimate2CbluetoothFix

cmake -B build -S .
cmake --build build --config Release
```

The binary is `build/Release/Ultimate2CFixer.exe`.

### Linux
Prerequisites: a C++20 compiler (GCC or Clang), CMake 3.20 or newer, the Linux kernel headers, Qt 6.4 or newer (optional, for the window) and libsystemd (optional, for the battery level).

```sh
git clone https://github.com/bezelye404/Ultimate2CbluetoothFix.git
cd Ultimate2CbluetoothFix
git checkout linux-port

cmake -B build -S linux
cmake --build build
ctest --test-dir build
```

The window is `build/app/ultimate2cfixer`. If Qt is not found, CMake prints a warning and builds everything else.

---

## 📝 License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

---

## 🙏 Acknowledgments

* [ViGEmBus](https://github.com/nefarius/ViGEmBus) and ViGEmClient by Benjamin Höglinger-Stelzer (Nefarius). The client source in this repository keeps its original MIT license header.
* [Qt 6](https://www.qt.io/), linked dynamically by the Linux window.
* The Linux kernel `evdev` and `uinput` interfaces, [BlueZ](https://www.bluez.org/) and `sd-bus` from systemd, used by the Linux version.
