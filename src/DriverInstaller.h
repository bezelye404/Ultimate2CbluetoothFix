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

} // namespace Ultimate2CFixer
