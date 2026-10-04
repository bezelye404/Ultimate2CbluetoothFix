// The main window. It draws what the ViewModel says and forwards clicks; it decides nothing itself.
#pragma once
#include <QWidget>

#include <QDialog>
#include <QPointer>

#include <deque>
#include <functional>
#include <string>
#include <vector>

#include "u2c/service.h"
#include "u2c/ui/view_model.h"

class QPainter;
class QPlainTextEdit;
class QTimer;

namespace u2c::uiqt {

class FlatButton;
class FlatCheck;

class MainWindow : public QWidget {
public:
    struct Hooks {
        std::function<void(const std::string& language_code)> language_selected;
        std::function<void(const Settings&)> settings_changed;  // apply to the service and save
        std::function<bool()> autostart_enabled;  // is the start-up entry present?
        std::function<bool(bool)> set_autostart;  // write or remove it; false = failed
        std::function<bool()> tray_available;  // is there a tray right now?
        std::function<void(const std::string&)> tooltip_changed;  // text for the tray icon
        std::function<void(const std::string& title, const std::string& message)> battery_alert;  // the low battery balloon
        std::function<void()> quit;
        std::function<ui::Distro()> distro;  // for the commands in the notice dialog
    };

    MainWindow(ui::ViewModel* vm, Service* service, std::vector<std::string> languages, Hooks hooks, QWidget* parent = nullptr);

    // Called in the GUI thread (ServiceBridge moves the service events here).
    void handle_status(ServiceStatus status, const std::string& device);
    void handle_battery(const BatteryInfo& battery);
    void handle_input(const PadReport& report);
    void handle_log(const LogEvent& event);
    void handle_notice(const Notice& notice);

    void refresh_texts();
    void show_settings(bool show);
    // Shows the window (also from the tray, or when a second copy was started).
    void bring_to_front();
    // Hides the window to the tray, or minimizes it when there is no tray.
    void hide_to_tray();
    void publish_tooltip();
    // A problem found by the window itself (no tray, no Wayland plugin). Shown like the service notices.
    void add_ui_notice(ui::UiNotice notice, const std::string& detail = std::string());

protected:
    void paintEvent(QPaintEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    void closeEvent(QCloseEvent* event) override;

private:
    void update_buttons();
    void update_log();
    void log_pending_notices();
    void present_notice_dialogs();
    void sync_settings_widgets();
    void settings_edited();
    void draw_settings(QPainter& p);
    void draw_header(QPainter& p);
    void draw_status_card(QPainter& p);
    void draw_telemetry_card(QPainter& p);
    void draw_battery_card(QPainter& p);
    void draw_terminal_card(QPainter& p);

    ui::ViewModel* vm_;
    Service* service_;
    std::vector<std::string> languages_;
    Hooks hooks_;

    FlatButton* start_ = nullptr;
    FlatButton* stop_ = nullptr;
    FlatButton* lang_ = nullptr;
    FlatButton* settings_ = nullptr;
    FlatButton* tray_ = nullptr;
    FlatButton* clear_ = nullptr;
    QPlainTextEdit* log_ = nullptr;
    QTimer* tick_ = nullptr;

    bool settings_view_ = false;
    FlatCheck* checks_[7] = {};
    FlatButton* deadzone_ = nullptr;
    FlatButton* polling_ = nullptr;
    FlatButton* curve_ = nullptr;
    FlatButton* back_ = nullptr;

    // notice dialogs: one after the other, only while the window is visible
    std::deque<ui::ViewModel::PendingNotice> dialog_queue_;
    QPointer<QDialog> dialog_;
};

}  // namespace u2c::uiqt
