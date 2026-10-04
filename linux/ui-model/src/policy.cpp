#include "u2c/ui/policy.h"

#include <algorithm>
#include <cctype>

namespace u2c::ui {

CloseAction on_close_requested(bool minimize_on_close, bool tray_available) {
    return (minimize_on_close && tray_available) ? CloseAction::HideToTray : CloseAction::Quit;
}

MinimizeAction on_tray_button(bool tray_available) {
    return tray_available ? MinimizeAction::HideToTray : MinimizeAction::MinimizeWindow;
}

StartupAction startup_action(bool start_minimized, bool tray_available) {
    if (!start_minimized) return StartupAction::ShowNormal;
    return tray_available ? StartupAction::HideToTray : StartupAction::ShowMinimized;
}

UiNotice ui_notice_from(NoticeKind kind) {
    switch (kind) {
        case NoticeKind::NoControllerAccess: return UiNotice::NoControllerAccess;
        case NoticeKind::NoVirtualDevice: return UiNotice::NoVirtualDevice;
        case NoticeKind::NoBluetoothService: return UiNotice::NoBluetoothService;
    }
    return UiNotice::NoBluetoothService;
}

namespace {

std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

// Value of KEY=value in os-release text (quotes removed), empty if missing.
std::string os_release_value(std::string_view text, std::string_view key) {
    size_t pos = 0;
    while (pos < text.size()) {
        const size_t nl = text.find('\n', pos);
        std::string_view line = text.substr(pos, nl == std::string_view::npos ? std::string_view::npos : nl - pos);
        pos = nl == std::string_view::npos ? text.size() : nl + 1;
        if (line.size() > key.size() + 1 && line.substr(0, key.size()) == key && line[key.size()] == '=') {
            std::string v(line.substr(key.size() + 1));
            while (!v.empty() && (v.back() == '\r' || v.back() == ' ')) v.pop_back();
            if (v.size() >= 2 && (v.front() == '"' || v.front() == '\'') && v.back() == v.front()) v = v.substr(1, v.size() - 2);
            return v;
        }
    }
    return {};
}

bool has_word(const std::string& list, const char* word) {
    const std::string w = word;
    size_t pos = 0;
    while (pos <= list.size()) {
        const size_t sp = list.find(' ', pos);
        const std::string item = list.substr(pos, sp == std::string::npos ? std::string::npos : sp - pos);
        if (item == w) return true;
        if (sp == std::string::npos) break;
        pos = sp + 1;
    }
    return false;
}

}  // namespace

Distro detect_distro(std::string_view os_release) {
    const std::string id = lower(os_release_value(os_release, "ID"));
    const std::string like = lower(os_release_value(os_release, "ID_LIKE"));
    if (id == "arch" || has_word(like, "arch")) return Distro::Arch;
    if (id == "debian" || id == "ubuntu" || has_word(like, "debian") || has_word(like, "ubuntu")) return Distro::Debian;
    return Distro::Other;
}

NoticeContent notice_content(UiNotice notice, Distro distro) {
    NoticeContent c;
    // The udev rule is the one Valve's steam-devices package ships for /dev/uinput; the user session gets access
    // through logind (uaccess). The other lines load the module now and at every boot and apply the rule.
    const std::vector<std::string> uinput_commands = {
        "echo 'KERNEL==\"uinput\", SUBSYSTEM==\"misc\", TAG+=\"uaccess\", OPTIONS+=\"static_node=uinput\"' | sudo tee /etc/udev/rules.d/60-ultimate2cfixer-uinput.rules",
        "echo uinput | sudo tee /etc/modules-load.d/ultimate2cfixer.conf",
        "sudo modprobe uinput",
        "sudo udevadm control --reload-rules && sudo udevadm trigger",
    };
    switch (notice) {
        case UiNotice::NoVirtualDevice:
            c.body_key = "NoticeNoVirtualDevice";
            if (distro != Distro::Other) c.commands = uinput_commands;
            break;
        case UiNotice::NoControllerAccess:
            c.body_key = "NoticeNoControllerAccess";
            if (distro != Distro::Other) {
                c.commands = {
                    "echo 'SUBSYSTEM==\"input\", KERNEL==\"event*\", ATTRS{id/vendor}==\"2dc8\", TAG+=\"uaccess\"' | sudo tee /etc/udev/rules.d/60-ultimate2cfixer-controller.rules",
                    "sudo udevadm control --reload-rules && sudo udevadm trigger",
                };
            }
            break;
        case UiNotice::NoBluetoothService:
            c.body_key = "NoticeNoBluetoothService";
            if (distro == Distro::Arch) c.commands = {"sudo pacman -S bluez bluez-utils", "sudo systemctl enable --now bluetooth"};
            else if (distro == Distro::Debian) c.commands = {"sudo apt install bluez", "sudo systemctl enable --now bluetooth"};
            break;
        case UiNotice::NoTrayHost:
            c.body_key = "NoticeNoTrayHost";
            if (distro == Distro::Arch) c.commands = {"sudo pacman -S gnome-shell-extension-appindicator"};
            else if (distro == Distro::Debian) c.commands = {"sudo apt install gnome-shell-extension-appindicator"};
            break;
        case UiNotice::WaylandPluginMissing:
            c.body_key = "NoticeWaylandPlugin";
            if (distro == Distro::Arch) c.commands = {"sudo pacman -S qt6-wayland"};
            else if (distro == Distro::Debian) c.commands = {"sudo apt install qt6-wayland"};
            break;
    }
    return c;
}

}  // namespace u2c::ui
