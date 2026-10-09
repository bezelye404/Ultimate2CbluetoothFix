#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <string>

namespace Ultimate2CFixer {

// Disconnects the connected controller by disabling and re-enabling its Bluetooth LE device node.
//
// There is no unelevated way to drop a Bluetooth LE connection (the Windows disconnect request only handles
// classic Bluetooth), so this asks for permission (UAC). The programs that were using the controller then see
// it disappear and arrive again, which is the only thing that makes them re-scan it.

// "BTHLE\DEV_<address>\..." of the Bluetooth device the connected controller uses; empty when it is not
// connected over Bluetooth LE.
std::wstring FindConnectedBluetoothNode(DWORD vid, DWORD pid);

// The cmd.exe arguments that disable the node, wait, and enable it again; empty when the id does not look
// like a Bluetooth LE device node.
std::wstring BuildReconnectCommand(const std::wstring& nodeId);

// Shows the Windows permission prompt and starts the disconnect. The helper runs on its own, so this
// application can exit right away. Returns false when nothing was started: no controller connected, or
// permission declined.
bool DisconnectController(DWORD vid, DWORD pid);

} // namespace Ultimate2CFixer
