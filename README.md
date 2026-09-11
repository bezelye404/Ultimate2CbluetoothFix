# 8BitDo Ultimate 2C Bluetooth Fixer (C++ Native)

A lightweight, native Windows C++ application that remaps 8BitDo Ultimate 2C controllers (connected via Bluetooth) to a virtual XInput (Xbox 360) controller with battery monitoring, auto-reconnect, dirty-checking, and system tray support.

![C++](https://img.shields.io/badge/C++-20-00599C?logo=c%2B%2B)
![Windows](https://img.shields.io/badge/Windows-10+-0078D6?logo=windows)
![License](https://img.shields.io/badge/License-MIT-green)

---

## ✨ Features

- 🎮 **DirectInput -> XInput Remapping**: Flawlessly maps the 8BitDo Ultimate 2C Bluetooth DirectInput layout to a virtual Xbox 360 controller via ViGEmBus.
- ⚡ **Ultra-Low Memory & CPU (~3 - 8 MB RAM)**: Pure C++20 with zero .NET runtime dependencies, garbage collector pauses, or heavy frameworks.
- 🔄 **Auto-Reconnect Loop**: Automatically detects when the controller turns on, disconnects, or goes to sleep, reconnecting seamlessly without manual restarts.
- 🎯 **Dirty-Checking Hot-Loop**: Avoids sending redundant kernel reports when the controller is idle, saving 98%+ of idle CPU calls.
- 🔋 **Battery Monitoring**: Reads real-time controller battery percentage via Bluetooth LE GATT services.
- 🗕 **System Tray Integration**: Minimizes to the Windows notification area with Working Set trimming.
- 🎨 **Minimal Dark UI**: Clean, neutral dark Win32 interface with Windows 10/11 Immersive Dark Mode support.
- 🌐 **English / Turkish**: Built-in dual language support with instant switching.

---

## 📋 Requirements

### For Users (Run)
* Windows 10 (Build 19041 or later)
* [ViGEmBus Driver](https://github.com/nefarius/ViGEmBus/releases) — **Required**

### For Developers (Build)
* Visual Studio 2019 / 2022 / 2026 (with "Desktop development with C++") OR CMake (3.20+) + MSVC / Clang / MinGW.

---

## 🔨 Building from Source

Using CMake:

```powershell
mkdir build
cd build
cmake ..
cmake --build . --config Release
```

The output executable `8bitdofixer.exe` will be generated in `build/Release/`.

---

## 🚀 Usage

1. Install the [ViGEmBus Driver](https://github.com/nefarius/ViGEmBus/releases).
2. Turn on your 8BitDo Ultimate 2C in **Bluetooth mode** and pair it with Windows.
3. Launch `8bitdofixer.exe`.
4. Click **Start Service** (or press the Tray button to hide to background).


* **Trigger Axes:** The triggers function as digital buttons. Another issue due to hardware limitations since Bluetooth mode is built considering Android at the first place.
---

## 📝 License

Distributed under the **MIT License**. Feel free to use and contribute to the project.

---

## 🙏 Credits

* [ViGEmBus](https://github.com/nefarius/ViGEmBus) by Nefarius
* [SharpDX](https://github.com/sharpdx/SharpDX)
