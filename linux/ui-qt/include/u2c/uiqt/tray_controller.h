// The tray icon with its menu (Open, Exit), tooltip and balloon messages, through Qt's StatusNotifierItem support.
#pragma once
#include <QIcon>
#include <QMenu>
#include <QString>
#include <QSystemTrayIcon>

#include <functional>

namespace u2c::uiqt {

class TrayController {
public:
    struct Callbacks {
        std::function<void()> open;  // "Open" in the menu or a click on the icon
        std::function<void()> exit;  // "Exit" in the menu
    };
    TrayController(const QIcon& icon, Callbacks callbacks);

    // False when no desktop component shows tray icons (for example GNOME without the AppIndicator extension).
    static bool available() { return QSystemTrayIcon::isSystemTrayAvailable(); }
    void show() { tray_.show(); }
    void set_texts(const QString& open, const QString& exit);
    void set_tooltip(const QString& text) { tray_.setToolTip(text); }
    void show_warning(const QString& title, const QString& message);

private:
    QSystemTrayIcon tray_;
    QMenu menu_;
    QAction* open_ = nullptr;
    QAction* exit_ = nullptr;
    Callbacks cb_;
};

}  // namespace u2c::uiqt
