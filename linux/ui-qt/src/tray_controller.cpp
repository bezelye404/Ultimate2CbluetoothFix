#include "u2c/uiqt/tray_controller.h"

namespace u2c::uiqt {

TrayController::TrayController(const QIcon& icon, Callbacks callbacks) : tray_(icon), cb_(std::move(callbacks)) {
    open_ = menu_.addAction(QStringLiteral("Open"), [this] { if (cb_.open) cb_.open(); });
    menu_.addSeparator();
    exit_ = menu_.addAction(QStringLiteral("Exit"), [this] { if (cb_.exit) cb_.exit(); });
    tray_.setContextMenu(&menu_);
    // A click on the icon opens the window.
    QObject::connect(&tray_, &QSystemTrayIcon::activated, [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick) {
            if (cb_.open) cb_.open();
        }
    });
}

void TrayController::set_texts(const QString& open, const QString& exit) {
    open_->setText(open);
    exit_->setText(exit);
}

void TrayController::show_warning(const QString& title, const QString& message) {
    tray_.showMessage(title, message, QSystemTrayIcon::Warning, 8000);
}

}  // namespace u2c::uiqt
