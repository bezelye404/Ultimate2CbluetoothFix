#include "u2c/uiqt/main_window.h"

#include <QCloseEvent>
#include <QDateTime>
#include <QTime>
#include <QFont>
#include <QFrame>
#include <QPalette>
#include <QFontMetrics>
#include <QPainter>
#include <QPlainTextEdit>
#include <QScrollBar>
#include <QTimer>

#include <initializer_list>

#include <malloc.h>

#include <chrono>

#include "u2c/ui/layout.h"
#include "u2c/ui/policy.h"
#include "u2c/ui/text_state.h"
#include "u2c/uiqt/colors.h"
#include "u2c/uiqt/flat_button.h"
#include "u2c/uiqt/notice_dialog.h"

namespace u2c::uiqt {

namespace {

using ui::ColorId;

QRect qrect(const ui::Rect& r) { return QRect(r.x, r.y, r.w, r.h); }

QFont make_font(const QFont& base, int pixel_size, QFont::Weight weight = QFont::Normal) {
    QFont f = base;
    f.setPixelSize(pixel_size);
    f.setWeight(weight);
    return f;
}

void round_rect(QPainter& p, const QRect& r, int radius, ColorId fill, ColorId border) {
    p.setPen(QPen(qcolor(border), 1));
    p.setBrush(qcolor(fill));
    p.drawRoundedRect(QRectF(r.x() + 0.5, r.y() + 0.5, r.width() - 1.0, r.height() - 1.0), radius, radius);
}

void draw_text(QPainter& p, const QRect& r, int flags, const QString& s, ColorId color) {
    p.setPen(qcolor(color));
    p.drawText(r, flags | Qt::TextSingleLine, s);
}

ui::ClockTime clock_now() {
    const QTime t = QDateTime::currentDateTime().time();
    return ui::ClockTime{t.hour(), t.minute(), t.second()};
}

int64_t now_ns() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

}  // namespace

MainWindow::MainWindow(ui::ViewModel* vm, Service* service, std::vector<std::string> languages, Hooks hooks, QWidget* parent)
    : QWidget(parent), vm_(vm), service_(service), languages_(std::move(languages)), hooks_(std::move(hooks)) {
    setFixedSize(ui::kWindowW, ui::kWindowH);
    setWindowTitle(QString::fromStdString(vm_->catalog().text("AppTitle")));

    start_ = new FlatButton(this, FlatButton::Style::Accent);
    start_->setGeometry(qrect(ui::kStartButton));
    stop_ = new FlatButton(this);
    stop_->setGeometry(qrect(ui::kStopButton));
    lang_ = new FlatButton(this);
    lang_->setGeometry(qrect(ui::kLangButton));
    settings_ = new FlatButton(this);
    settings_->setGeometry(qrect(ui::kSettingsButton));
    settings_->setText(QString(QChar(0x2699)));
    tray_ = new FlatButton(this);
    tray_->setGeometry(qrect(ui::kTrayButton));
    tray_->setText(QStringLiteral("_"));
    clear_ = new FlatButton(this);
    clear_->setGeometry(qrect(ui::kClearButton));

    log_ = new QPlainTextEdit(this);
    log_->setReadOnly(true);
    log_->setFrameStyle(QFrame::NoFrame);
    log_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    log_->setGeometry(qrect(ui::kLogBox));
    log_->setFont(make_font(QFont(QStringLiteral("monospace")), 13));
    QPalette lp = log_->palette();
    lp.setColor(QPalette::Base, qcolor(ColorId::CardBg));
    lp.setColor(QPalette::Text, qcolor(ColorId::TextSecondary));
    log_->setPalette(lp);

    connect(start_, &QAbstractButton::clicked, this, [this] { service_->start(); update_buttons(); });
    connect(stop_, &QAbstractButton::clicked, this, [this] { service_->stop(); update_buttons(); });
    connect(clear_, &QAbstractButton::clicked, this, [this] { vm_->clear_log(); update_log(); });
    connect(lang_, &QAbstractButton::clicked, this, [this] {
        if (hooks_.language_selected) hooks_.language_selected(ui::next_language(vm_->active_language(), languages_));
    });
    connect(settings_, &QAbstractButton::clicked, this, [this] { show_settings(!settings_view_); });
    connect(tray_, &QAbstractButton::clicked, this, [this] { hide_to_tray(); });

    for (int i = 0; i < ui::kCheckboxCount; ++i) {
        FlatCheck* box = new FlatCheck(this);
        box->setGeometry(ui::kCheckboxX, ui::checkbox_y(i), ui::kCheckboxW, ui::kCheckboxH);
        box->hide();
        checks_[i] = box;
        connect(box, &QAbstractButton::toggled, this, [this, i](bool on) {
            const ui::SettingId id = ui::kCheckboxes[i].id;
            if (id == ui::SettingId::StartWithSystem) {
                if (hooks_.set_autostart && !hooks_.set_autostart(on)) {  // it could not be written: show the real state again
                    handle_log(LogEvent{"LogAutostartFailed", {}});
                    sync_settings_widgets();
                }
                return;
            }
            vm_->set_checkbox(id, on);
            settings_edited();
        });
    }
    checks_[ui::kCheckboxCount - 1]->setToolTip(QString::fromStdString(vm_->catalog().text("ExclusiveGrabHelp")));

    deadzone_ = new FlatButton(this);
    deadzone_->setGeometry(qrect(ui::kDeadzoneButton));
    polling_ = new FlatButton(this);
    polling_->setGeometry(qrect(ui::kPollingButton));
    curve_ = new FlatButton(this);
    curve_->setGeometry(qrect(ui::kCurveButton));
    back_ = new FlatButton(this, FlatButton::Style::Accent);
    back_->setGeometry(qrect(ui::kBackButton));
    for (FlatButton* b : {deadzone_, polling_, curve_, back_}) b->hide();
    connect(deadzone_, &QAbstractButton::clicked, this, [this] { vm_->cycle_deadzone(); settings_edited(); });
    connect(polling_, &QAbstractButton::clicked, this, [this] { vm_->cycle_polling_rate(); settings_edited(); });
    connect(curve_, &QAbstractButton::clicked, this, [this] { vm_->cycle_curve(); settings_edited(); });
    connect(back_, &QAbstractButton::clicked, this, [this] { show_settings(false); });

    // The update rate readout needs a once-a-second look at the service while the window is visible.
    tick_ = new QTimer(this);
    tick_->setInterval(1000);
    connect(tick_, &QTimer::timeout, this, [this] {
        vm_->poll(service_->snapshot(), now_ns());
        update(QRect(ui::kCardTelemetry.x, ui::kCardTelemetry.y, ui::kCardTelemetry.w, ui::kCardTelemetry.h));
    });

    refresh_texts();
}

void MainWindow::refresh_texts() {
    const auto& c = vm_->catalog();
    setWindowTitle(QString::fromStdString(c.text("AppTitle")));
    start_->setText(QString::fromStdString(c.text("StartBtn")));
    stop_->setText(QString::fromStdString(c.text("StopBtn")));
    clear_->setText(QString::fromStdString(c.text("ClearBtn")));
    lang_->setText(QString::fromStdString(ui::language_button_text(vm_->active_language(), languages_)));
    for (int i = 0; i < ui::kCheckboxCount; ++i) checks_[i]->setText(QString::fromStdString(c.text(ui::kCheckboxes[i].key)));
    checks_[ui::kCheckboxCount - 1]->setToolTip(QString::fromStdString(c.text("ExclusiveGrabHelp")));
    back_->setText(QString::fromStdString(c.text("SettingsBack")));
    sync_settings_widgets();
    update_buttons();
    update_log();
    publish_tooltip();
    update();
}

void MainWindow::sync_settings_widgets() {
    // set the controls from the model without sending the changes back
    for (int i = 0; i < ui::kCheckboxCount; ++i) {
        const ui::SettingId id = ui::kCheckboxes[i].id;
        const bool on = id == ui::SettingId::StartWithSystem ? (hooks_.autostart_enabled && hooks_.autostart_enabled())
                                                              : ui::get_setting(vm_->settings(), id);
        checks_[i]->blockSignals(true);
        checks_[i]->setChecked(on);
        checks_[i]->blockSignals(false);
    }
    deadzone_->setText(QString::fromStdString(vm_->deadzone_label()));
    polling_->setText(QString::fromStdString(vm_->polling_label()));
    curve_->setText(QString::fromStdString(vm_->curve_label()));
}

void MainWindow::settings_edited() {
    sync_settings_widgets();
    if (hooks_.settings_changed) hooks_.settings_changed(vm_->settings());
    update();
}

void MainWindow::show_settings(bool show) {
    settings_view_ = show;
    for (QWidget* w : std::initializer_list<QWidget*>{start_, stop_, clear_, log_}) w->setVisible(!show);
    for (int i = 0; i < ui::kCheckboxCount; ++i) checks_[i]->setVisible(show);
    for (FlatButton* b : {deadzone_, polling_, curve_, back_}) b->setVisible(show);
    if (show) sync_settings_widgets();
    update();
}

void MainWindow::update_buttons() {
    const bool running = service_->running();
    start_->setEnabled(!running);
    stop_->setEnabled(running);
}

void MainWindow::update_log() {
    log_->setPlainText(QString::fromStdString(vm_->log_text()));
    log_->verticalScrollBar()->setValue(log_->verticalScrollBar()->maximum());
}

void MainWindow::handle_status(ServiceStatus status, const std::string& device) {
    vm_->on_status(status, device);
    update_buttons();
    publish_tooltip();
    update();
}

void MainWindow::handle_battery(const BatteryInfo& battery) {
    vm_->on_battery(battery);
    publish_tooltip();
    if (vm_->take_battery_alert() && hooks_.battery_alert) {  // once at 15% or lower, armed again above 20%
        hooks_.battery_alert(vm_->catalog().text("LowBatteryAlertTitle"),
                             vm_->catalog().format("LowBatteryAlertMsg", {{"level", std::to_string(vm_->battery_level())}}));
    }
    update(QRect(ui::kCardBattery.x, ui::kCardBattery.y, ui::kCardBattery.w, ui::kCardBattery.h));
}

void MainWindow::handle_input(const PadReport& report) {
    vm_->on_input(report);
    update(QRect(ui::kCardTelemetry.x, ui::kCardTelemetry.y, ui::kCardTelemetry.w, ui::kCardTelemetry.h));
}

void MainWindow::handle_log(const LogEvent& event) {
    vm_->on_log(event, clock_now());
    update_log();
}

void MainWindow::handle_notice(const Notice& notice) {
    vm_->on_notice(notice);
    log_pending_notices();
}

// Each problem goes to the log in plain words and into a dialog (one after the other, while the window is visible).
void MainWindow::log_pending_notices() {
    while (auto n = vm_->take_notice()) {
        vm_->on_log(LogEvent{ui::notice_content(n->kind, ui::Distro::Other).body_key, {{"detail", n->detail}}}, clock_now());
        dialog_queue_.push_back(*n);
    }
    update_log();
    present_notice_dialogs();
}

void MainWindow::present_notice_dialogs() {
    if (!isVisible() || dialog_ || dialog_queue_.empty()) return;
    const ui::ViewModel::PendingNotice n = dialog_queue_.front();
    dialog_queue_.pop_front();
    const ui::Distro distro = hooks_.distro ? hooks_.distro() : ui::Distro::Other;
    auto* d = new NoticeDialog(vm_->catalog(), n.kind, n.detail, distro, this);
    dialog_ = d;
    connect(d, &QObject::destroyed, this, [this] { QTimer::singleShot(0, this, [this] { present_notice_dialogs(); }); });
    d->show();
}

void MainWindow::add_ui_notice(ui::UiNotice notice, const std::string& detail) {
    vm_->add_notice(notice, detail);
    log_pending_notices();
}

void MainWindow::publish_tooltip() {
    if (hooks_.tooltip_changed) hooks_.tooltip_changed(vm_->tooltip());
}

void MainWindow::bring_to_front() {
    if (isMinimized()) showNormal();
    show();
    raise();
    activateWindow();
}

void MainWindow::hide_to_tray() {
    const bool tray = hooks_.tray_available ? hooks_.tray_available() : false;
    vm_->set_tray_available(tray);
    if (ui::on_tray_button(tray) == ui::MinimizeAction::HideToTray) hide();
    else showMinimized();
}

void MainWindow::closeEvent(QCloseEvent* e) {
    // Hiding on close only works while a tray exists: a hidden window could never be reopened.
    const bool tray = hooks_.tray_available ? hooks_.tray_available() : false;
    vm_->set_tray_available(tray);
    if (ui::on_close_requested(vm_->settings().minimize_on_close, tray) == ui::CloseAction::HideToTray) {
        e->ignore();
        hide();
        return;
    }
    e->accept();
    if (hooks_.quit) hooks_.quit();
}

void MainWindow::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    tick_->start();
    QTimer::singleShot(0, this, [this] { present_notice_dialogs(); });
}

void MainWindow::hideEvent(QHideEvent* e) {
    QWidget::hideEvent(e);
    tick_->stop();  // nothing runs while the window is hidden
    // Give freed memory back once the window is gone (measured: about 2.5 MB less). Only if the window is still
    // hidden half a second later.
    QTimer::singleShot(500, this, [this] { if (!isVisible()) malloc_trim(0); });
}

void MainWindow::paintEvent(QPaintEvent* event) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);
    // Only the exposed area is drawn: a live input update repaints just the telemetry card, so the other cards
    // are not laid out again for every controller update.
    const QRect dirty = event->rect();
    p.fillRect(dirty, qcolor(ColorId::WindowBg));
    if (dirty.intersects(QRect(0, 0, width(), 56))) draw_header(p);
    if (settings_view_) { draw_settings(p); return; }
    if (dirty.intersects(qrect(ui::kCardStatus))) draw_status_card(p);
    if (dirty.intersects(qrect(ui::kCardTelemetry))) draw_telemetry_card(p);
    if (dirty.intersects(qrect(ui::kCardBattery))) draw_battery_card(p);
    if (dirty.intersects(qrect(ui::kCardTerminal))) draw_terminal_card(p);
}

void MainWindow::draw_header(QPainter& p) {
    p.setFont(make_font(font(), 20, QFont::DemiBold));
    draw_text(p, QRect(ui::kTitleX, ui::kTitleY, 400, 30), Qt::AlignLeft | Qt::AlignTop,
              QString::fromStdString(vm_->catalog().text("AppTitle")), ColorId::TextPrimary);
}

void MainWindow::draw_settings(QPainter& p) {
    round_rect(p, qrect(ui::kSettingsCard), 8, ColorId::CardBg, ColorId::CardBorder);
    p.setFont(make_font(font(), 20, QFont::DemiBold));
    draw_text(p, QRect(ui::kSettingsTitleX, ui::kSettingsTitleY, 400, 30), Qt::AlignLeft | Qt::AlignTop,
              QString::fromStdString(vm_->catalog().text("SettingsTitle")), ColorId::TextPrimary);
}

void MainWindow::draw_status_card(QPainter& p) {
    const ui::Rect c = ui::kCardStatus;
    round_rect(p, qrect(c), 8, ColorId::CardBg, ColorId::CardBorder);
    const auto& cat = vm_->catalog();
    p.setFont(make_font(font(), 12));
    draw_text(p, QRect(c.x + ui::kCaptionOffsetX, c.y + ui::kCaptionOffsetY, c.w - 32, 16), Qt::AlignLeft | Qt::AlignTop,
              QString::fromStdString(cat.text("StatusTitle")), ColorId::TextMuted);

    const ui::StatusView sv = vm_->status();
    const QFont title_font = make_font(font(), 20, QFont::DemiBold);
    p.setFont(title_font);
    const QString name = QFontMetrics(title_font).elidedText(QString::fromStdString(sv.name_text), Qt::ElideRight, c.w - 32);
    draw_text(p, QRect(c.x + ui::kCaptionOffsetX, c.y + ui::kStatusNameOffsetY, c.w - 32, 24), Qt::AlignLeft | Qt::AlignVCenter, name,
              ColorId::TextPrimary);

    p.setFont(make_font(font(), 15));
    const int y = c.y + ui::kStatusLineOffsetY;
    draw_text(p, QRect(c.x + ui::kCaptionOffsetX, y, 16, 20), Qt::AlignLeft | Qt::AlignTop, QString(QChar(0x25CF)), sv.dot);
    draw_text(p, QRect(c.x + ui::kStatusTextOffsetX, y, c.w - ui::kStatusTextOffsetX - 8, 20), Qt::AlignLeft | Qt::AlignTop,
              QString::fromStdString(sv.status_text), ColorId::TextSecondary);
}

void MainWindow::draw_telemetry_card(QPainter& p) {
    const ui::Rect c = ui::kCardTelemetry;
    round_rect(p, qrect(c), 8, ColorId::CardBg, ColorId::CardBorder);
    p.setFont(make_font(font(), 12));
    draw_text(p, QRect(c.x + ui::kCaptionOffsetX, c.y + ui::kCaptionOffsetY, 150, 16), Qt::AlignLeft | Qt::AlignTop,
              QString::fromStdString(vm_->catalog().text("LiveInputTitle")), ColorId::TextMuted);

    const ui::BadgeView badge = vm_->badge();
    const QRect badge_rect(c.x + ui::kBadge.x, c.y + ui::kBadge.y, ui::kBadge.w, ui::kBadge.h);
    round_rect(p, badge_rect, 4, ColorId::BadgeBg, ColorId::CardBorder);
    p.setFont(make_font(font(), 12));
    draw_text(p, badge_rect, Qt::AlignCenter, QString::fromLatin1(badge.text), badge.color);

    if (vm_->rate_visible()) {
        draw_text(p, QRect(c.x + ui::kCaptionOffsetX, c.y + ui::kRateTextOffsetY, 160, 16), Qt::AlignLeft | Qt::AlignTop,
                  QString::fromStdString(vm_->rate(now_ns())), ColorId::TextMuted);
    }

    const int vx = c.x + ui::kVisOffsetX, vy = c.y + ui::kVisOffsetY;
    const PadReport& in = vm_->input();

    const int16_t sx[2] = {in.lx, in.rx}, sy[2] = {in.ly, in.ry};
    for (int s = 0; s < 2; ++s) {
        const QRect box(vx + s * ui::kStickBoxGap, vy, ui::kStickBoxSize, ui::kStickBoxSize);
        round_rect(p, box, 4, ColorId::StickBg, ColorId::CardBorder);
        p.setPen(QPen(qcolor(ColorId::Cross), 1));
        const int cx = box.x() + ui::kStickCenter, cy = box.y() + ui::kStickCenter;
        p.drawLine(cx, box.y() + 6, cx, box.bottom() - 6);
        p.drawLine(box.x() + 6, cy, box.right() - 6, cy);
        const ui::Offset o = ui::stick_dot_offset(sx[s], sy[s]);
        p.setPen(Qt::NoPen);
        p.setBrush(qcolor(ColorId::Green));
        p.drawEllipse(QPoint(cx + o.dx, cy + o.dy), ui::kDotRadius, ui::kDotRadius);
    }

    const auto tiles = vm_->tiles();
    p.setFont(make_font(font(), 12));
    for (int i = 0; i < 4; ++i) {
        const QRect r(vx + ui::kAbxyOriginX + ui::kTileDx[i], vy + ui::kTileDy[i], ui::kTileSize, ui::kTileSize);
        const bool lit = tiles[static_cast<size_t>(i)].lit;
        round_rect(p, r, 3, lit ? ColorId::Green : ColorId::TileBg, lit ? ColorId::Green : ColorId::CardBorder);
        draw_text(p, r, Qt::AlignCenter, QString(QChar(tiles[static_cast<size_t>(i)].label)), lit ? ColorId::OnGreen : ColorId::TextMuted);
    }

    for (int i = 0; i < 2; ++i) {
        const bool lit = ui::bumper_lit(in.buttons, i == 0);
        const QRect r(vx + ui::kBumper[i].x, vy + ui::kBumper[i].y, ui::kBumper[i].w, ui::kBumper[i].h);
        round_rect(p, r, 4, lit ? ColorId::Green : ColorId::TileBg, lit ? ColorId::Green : ColorId::CardBorder);
        draw_text(p, r, Qt::AlignCenter, i == 0 ? QStringLiteral("LB") : QStringLiteral("RB"), lit ? ColorId::OnGreen : ColorId::TextMuted);
    }

    const uint8_t tv[2] = {in.left_trigger, in.right_trigger};
    for (int i = 0; i < 2; ++i) {
        const QRect track(vx + ui::kTriggerX[i], vy + ui::kTriggerY, ui::kTriggerW, ui::kTriggerH);
        p.fillRect(track, qcolor(ColorId::StickBg));
        const int h = ui::trigger_fill_height(tv[i]);
        if (h > 0) p.fillRect(QRect(track.x(), track.bottom() + 1 - h, track.width(), h), qcolor(ColorId::Green));
    }
}

void MainWindow::draw_battery_card(QPainter& p) {
    const ui::Rect c = ui::kCardBattery;
    round_rect(p, qrect(c), 8, ColorId::CardBg, ColorId::CardBorder);
    p.setFont(make_font(font(), 12));
    draw_text(p, QRect(c.x + ui::kCaptionOffsetX, c.y + ui::kCaptionOffsetY, 100, 16), Qt::AlignLeft | Qt::AlignTop,
              QString::fromStdString(vm_->catalog().text("BatteryTitle")), ColorId::TextMuted);

    const ui::BatteryView bv = vm_->battery();
    p.setFont(make_font(font(), 20, QFont::DemiBold));
    draw_text(p, QRect(c.x + 16, c.y + ui::kBatteryPercentOffsetY, c.w - 32, 26), Qt::AlignRight | Qt::AlignTop,
              QString::fromStdString(bv.percent_text), ColorId::TextPrimary);

    const QRect track(c.x + 16, c.y + ui::kBatteryTrackOffsetY, c.w - 32, ui::kBatteryTrackH);
    p.fillRect(track, qcolor(ColorId::CardBorder));
    const int fill = ui::battery_fill_width(track.width(), bv.fill_percent);
    if (fill > 0) p.fillRect(QRect(track.x(), track.y(), fill, track.height()), qcolor(bv.bar_color));

    const QFont small = make_font(font(), 12);
    p.setFont(small);
    const QString text = QFontMetrics(small).elidedText(QString::fromStdString(bv.status_text), Qt::ElideRight, c.w - 32);
    draw_text(p, QRect(c.x + 16, c.y + ui::kBatteryTextOffsetY, c.w - 32, 20), Qt::AlignLeft | Qt::AlignVCenter, text, bv.status_color);
}

void MainWindow::draw_terminal_card(QPainter& p) {
    round_rect(p, qrect(ui::kCardTerminal), 8, ColorId::CardBg, ColorId::CardBorder);
    p.setFont(make_font(font(), 12));
    draw_text(p, QRect(ui::kTerminalCaptionX, ui::kTerminalCaptionY, 300, 16), Qt::AlignLeft | Qt::AlignTop,
              QString::fromStdString(vm_->catalog().text("LogsTitle")), ColorId::TextMuted);
}

}  // namespace u2c::uiqt
