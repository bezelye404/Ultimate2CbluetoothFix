// Rules for closing, minimizing and starting the window, and the content of the dependency notices.
#pragma once
#include <string>
#include <string_view>
#include <vector>

#include "u2c/service.h"

namespace u2c::ui {

// The X button. Without a tray the window cannot be reopened once hidden, so it quits.
enum class CloseAction { HideToTray, Quit };
CloseAction on_close_requested(bool minimize_on_close, bool tray_available);

// The "_" button of the top bar.
enum class MinimizeAction { HideToTray, MinimizeWindow };
MinimizeAction on_tray_button(bool tray_available);

// Start with --minimized.
enum class StartupAction { ShowNormal, HideToTray, ShowMinimized };
StartupAction startup_action(bool start_minimized, bool tray_available);

enum class UiNotice { NoControllerAccess, NoVirtualDevice, NoBluetoothService, NoTrayHost, WaylandPluginMissing };
UiNotice ui_notice_from(NoticeKind kind);

enum class Distro { Arch, Debian, Other };
// From the content of /etc/os-release (ID and ID_LIKE): arch-like, debian-like (Ubuntu, Mint ...) or other.
Distro detect_distro(std::string_view os_release);

struct NoticeContent {
    std::string body_key;  // translation key; may contain {detail}
    std::vector<std::string> commands;  // shown to copy, never run by the program; empty for an unknown distribution
    bool commands_unverified = true;  // the user is told the commands were not tested on every system
};
// `detail` fills {detail} in the body (for example the device path).
NoticeContent notice_content(UiNotice notice, Distro distro);

// Every translation key the notice dialog uses (a test checks them against the language files).
inline constexpr const char* kNoticeUiKeys[] = {
    "NoticeTitle", "NoticeHowToFix", "NoticeNoCommand", "NoticeCommandsUnverified", "NoticeCopy", "NoticeCopied", "NoticeClose",
    "NoticeNoControllerAccess", "NoticeNoVirtualDevice", "NoticeNoBluetoothService", "NoticeNoTrayHost", "NoticeWaylandPlugin",
};

}  // namespace u2c::ui
