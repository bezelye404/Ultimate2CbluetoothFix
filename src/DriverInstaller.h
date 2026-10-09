#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <string>
#include <functional>

namespace Ultimate2CFixer {

// MARK: - Driver Detection & Installation
bool IsViGEmBusInstalled();

using DriverProgressCb = std::function<void(int percent, const std::wstring& status)>;
using DriverFinishedCb = std::function<void(bool success, const std::wstring& message)>;

void StartViGEmBusInstall(HWND hwnd, DriverProgressCb onProgress, DriverFinishedCb onFinished);

// MARK: - Optional hiding driver (HidHide)
enum class HidHideInstallResult {
    Installed,
    DownloadFailed,   // Network problem; nothing was run.
    NotVerified,      // Downloaded file lacks a valid publisher signature; nothing was run.
    Cancelled,        // Windows permission prompt was declined.
    NeedsRestart      // Setup finished but the driver is not active yet.
};

using HidHideFinishedCb = std::function<void(HidHideInstallResult result)>;

void StartHidHideInstall(HWND hwnd, HidHideFinishedCb onFinished);

} // namespace Ultimate2CFixer
