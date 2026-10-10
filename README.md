# Ultimate2CFixer

![C++20](https://img.shields.io/badge/C%2B%2B-20-00599C?logo=c%2B%2B&logoColor=white)
![Platform](https://img.shields.io/badge/Platform-Windows%2010%20%2F%2011%20(x64)-0078D6?logo=windows&logoColor=white)
![License](https://img.shields.io/badge/License-MIT-success)

Makes the 8BitDo Ultimate 2C work like a normal Xbox controller on Windows when it is connected over Bluetooth.

[What it does](#what-it-does) | [Features](#features) | [Getting started](#getting-started) | [How the controls are mapped](#how-the-controls-are-mapped) | [Building from source](#building-from-source) | [License](#license)

---

## What it does

Over Bluetooth, Windows sees the Ultimate 2C as a DirectInput device, not an XInput one. Most modern games only understand XInput, so the controller either does nothing or behaves strangely (the right stick lands on odd axes, the triggers act like buttons).

Ultimate2CFixer reads the controller, translates every input, and passes it to a virtual Xbox 360 controller (through ViGEmBus). Games just see a regular Xbox pad.

It also hides the real controller from other programs while it runs. Without that, Steam and games would see two controllers, the real one and the virtual one, and every press would arrive twice.

---

## Features

**Controls**
* Both sticks, the D-pad, all buttons and the triggers are mapped to a virtual Xbox 360 pad.
* The triggers are analog: how far you pull is what the game gets. The digital click of the trigger is only used if the controller does not report an analog pull, so it never forces the full value.
* Hair Trigger (off by default): any pull past the deadzone counts as fully pressed.
* Three stick response curves: Linear, Smooth Aim and Aggressive. Adjustable deadzone.
* Nintendo layout option that swaps A/B and X/Y.
* Maximum update rate of 125, 250, 500 or 1000 Hz. The app only wakes up when the controller sends something, so an idle controller costs almost nothing.

**No double input**
* While the app is open, the real controller is hidden from games and Steam (using the [HidHide](https://github.com/nefarius/HidHide) driver, which the app offers to install). This is always on, there is no setting for it.
* The controller is hidden before it connects, stays hidden when you stop and start the service, and the hiding is put back within half a second if another program removes it. Closing the app gives the controller back.

**Everyday use**
* Battery level of the controller.
* The X button closes the app. The `_` button sends it to the system tray.
* Optional: switch the controller off when the app closes, so programs that were already using it notice. Windows asks for permission, and only when you close the app.
* Starts with Windows if you want it to. Only one copy can run at a time.
* Live view of the sticks, buttons and triggers, and a console that colours important lines: green for good news, red for problems, yellow when something needs your attention.
* English, Turkish and Spanish. The language button shows the current language and cycles through them.
* Settings are saved per user in the registry (`HKCU\Software\Ultimate2CFixer\Settings`).
* Small and light: the executable is around 330 KB, and it sits at a few megabytes of memory in the tray.

---

## Getting started

You need Windows 10 or 11 (64-bit) and a Bluetooth-paired 8BitDo Ultimate 2C. The app asks to install the two drivers it depends on if they are missing:

* [ViGEmBus](https://github.com/nefarius/ViGEmBus), which provides the virtual Xbox controller.
* [HidHide](https://github.com/nefarius/HidHide), which hides the real controller from other programs.

1. Download `Ultimate2CFixer.exe` from the [Releases](https://github.com/bezelye404/Ultimate2CbluetoothFix/releases) page. The file is not code signed yet, so Windows SmartScreen may show a warning the first time.
2. Pair the controller in Windows Bluetooth settings.
3. Run `Ultimate2CFixer.exe`. The service starts on its own; use **Start Service** and **Stop Service** if you want to control it by hand.

**One thing to know about hiding.** A program that already had the controller open when the hiding started (a browser tab, Steam) keeps it. Windows gives no signal that something changed. If you open the app while the controller is already on, the app reminds you: switch the controller off and on once, and reload the browser page. If the app starts with Windows, before the controller connects, you never run into this.

---

## How the controls are mapped

These are the DirectInput inputs of the Ultimate 2C over Bluetooth and where they end up:

| DirectInput | On the virtual Xbox pad |
| --- | --- |
| `lX`, `lY` | Left stick |
| `lZ`, `lRz` | Right stick (not `lRx`/`lRy`) |
| Brake axis | Left trigger, analog |
| Accelerator axis | Right trigger, analog |
| Buttons 6 and 7 | Left and right bumper |
| Buttons 8 and 9 | Trigger click (fallback only) |

The two trigger axes are HID usages (Brake and Accelerator). DirectInput's standard `DIJOYSTATE2` layout has no slot for them, so the Windows app reads the controller with its own data format that picks every control by its HID usage.

---

## Building from source

You need Visual Studio 2022 with the C++ desktop workload and CMake 3.20 or newer.

```powershell
git clone https://github.com/bezelye404/Ultimate2CbluetoothFix.git
cd Ultimate2CbluetoothFix
cmake -B build -S .
cmake --build build --config Release
```

The executable ends up in `build/Release/Ultimate2CFixer.exe`.

Two small tools help during development: `InputProbe` shows what the controller and HidHide are doing, and `tools/verify.ps1` builds the app, checks that every text fits in all languages, and measures idle CPU and memory. Build `InputProbe` and `LayoutCheck` with `cmake --build build --config Release --target InputProbe` (or `LayoutCheck`).

---

## License

MIT, see [LICENSE](LICENSE). Third-party license texts are in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

Thanks to [Nefarius](https://github.com/nefarius) for ViGEmBus and HidHide, which this project builds on.
