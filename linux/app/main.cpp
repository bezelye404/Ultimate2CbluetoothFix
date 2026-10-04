// ultimate2cfixer: the window. Options (for development and screenshots):
//   --demo                      fake service (no controller, nothing is written)
//   --demo-state <name>         freeze a scene: connected | lowbattery | searching | stopped | nintendo (with --demo)
//   --screenshot <file.png>     draw the window once, save it and quit (works without a display)
//   --settings <file>           settings file (default: ~/.config/ultimate2cfixer/settings.conf)
//   --simulate-notice <name>    show a notice dialog: access | uinput | bluetooth | tray | wayland
//   --distro <arch|debian|other>  distribution for the commands in the dialog (default: from /etc/os-release)
//   --measure-signals           SIGUSR1 = malloc_trim, SIGUSR2 = hide or show the window (RAM measurement)
//   --view settings             open the settings screen at start
//   --language <en|tr|es>       language for this run (not saved)
//   --lang-dir <dir>            translation files (default: the "lang" folder next to the program or in the data folders)
//   --input-dir <dir> --uinput <path>   device paths (testing)
//   --minimized                 start hidden in the tray (minimized when there is no tray)
#include <QApplication>
#include <QDialog>
#include <QFileInfo>
#include <QIcon>
#include <QLocale>
#include <QPalette>
#include <QPainter>
#include <QPixmap>
#include <QSocketNotifier>
#include <QStyleFactory>
#include <QTimer>

#include <algorithm>
#include <fcntl.h>
#include <malloc.h>
#include <signal.h>
#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "u2c/fake_service.h"
#include "u2c/platform/autostart.h"
#include "u2c/platform/linux_backend.h"
#include "u2c/platform/linux_service.h"
#include "u2c/platform/paths.h"
#include "u2c/platform/single_instance.h"
#include "u2c/platform/settings_store.h"
#include "u2c/ui/demo.h"
#include "u2c/ui/policy.h"
#include "u2c/ui/text_state.h"
#include "u2c/ui/view_model.h"
#include "u2c/uiqt/colors.h"
#include "u2c/uiqt/main_window.h"
#include "u2c/uiqt/service_bridge.h"
#include "u2c/uiqt/tray_controller.h"

using namespace u2c;

namespace {

struct Args {
    bool demo = false;
    bool minimized = false;
    std::string demo_state;
    std::string simulate_notice;
    std::string distro;
    bool measure_signals = false;
    std::string view;
    std::string language;
    std::string screenshot;
    std::string settings_path;
    std::string lang_dir;  // empty: looked up with find_data_path("lang")
    platform::LinuxBackendPaths paths;
};

Args parse_args(const QStringList& list) {
    Args a;
    for (int i = 1; i < list.size(); ++i) {
        const std::string s = list[i].toStdString();
        auto value = [&](std::string& out) { if (i + 1 < list.size()) out = list[++i].toStdString(); };
        if (s == "--demo") a.demo = true;
        else if (s == "--minimized" || s == "-m") a.minimized = true;
        else if (s == "--demo-state") value(a.demo_state);
        else if (s == "--language") value(a.language);
        else if (s == "--view") value(a.view);
        else if (s == "--simulate-notice") value(a.simulate_notice);
        else if (s == "--distro") value(a.distro);
        else if (s == "--measure-signals") a.measure_signals = true;
        else if (s == "--screenshot") value(a.screenshot);
        else if (s == "--settings") value(a.settings_path);
        else if (s == "--lang-dir") value(a.lang_dir);
        else if (s == "--input-dir") value(a.paths.input_dir);
        else if (s == "--uinput") value(a.paths.uinput);
    }
    return a;
}

std::string read_file(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    std::stringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

Translations load_language(const std::string& dir, const std::string& code) { return Translations::parse(read_file(dir + "/" + code + ".txt")); }

void apply_dark_palette(QApplication& app) {
    using ui::ColorId;
    QPalette p;
    p.setColor(QPalette::Window, uiqt::qcolor(ColorId::WindowBg));
    p.setColor(QPalette::WindowText, uiqt::qcolor(ColorId::TextPrimary));
    p.setColor(QPalette::Base, uiqt::qcolor(ColorId::CardBg));
    p.setColor(QPalette::AlternateBase, uiqt::qcolor(ColorId::CardBg));
    p.setColor(QPalette::Text, uiqt::qcolor(ColorId::TextSecondary));
    p.setColor(QPalette::Button, uiqt::qcolor(ColorId::ButtonBg));
    p.setColor(QPalette::ButtonText, uiqt::qcolor(ColorId::TextPrimary));
    p.setColor(QPalette::ToolTipBase, uiqt::qcolor(ColorId::CardBg));
    p.setColor(QPalette::ToolTipText, uiqt::qcolor(ColorId::TextPrimary));
    p.setColor(QPalette::PlaceholderText, uiqt::qcolor(ColorId::TextMuted));
    p.setColor(QPalette::Highlight, uiqt::qcolor(ColorId::AccentBorder));
    p.setColor(QPalette::HighlightedText, uiqt::qcolor(ColorId::AccentText));
    p.setColor(QPalette::Disabled, QPalette::Text, uiqt::qcolor(ColorId::TextMuted));
    p.setColor(QPalette::Disabled, QPalette::ButtonText, uiqt::qcolor(ColorId::TextMuted));
    app.setPalette(p);
}

// The icon of the window and the tray: the logo, or a plain green disc if it cannot be loaded.
QIcon make_icon() {
    const QString path = QString::fromStdString(platform::find_data_path("logo.ico"));
    if (!path.isEmpty()) {
        QIcon icon(path);
        if (!icon.isNull() && !icon.availableSizes().isEmpty()) return icon;
    }
    QPixmap pix(32, 32);
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(Qt::NoPen);
    p.setBrush(uiqt::qcolor(ui::ColorId::Green));
    p.drawEllipse(4, 4, 24, 24);
    return QIcon(pix);
}

// Scenes for screenshots and the live demo, driven through the fake service (GUI thread only).
class Demo {
public:
    Demo(FakeService* svc, ui::ViewModel* vm, const std::string& state) : svc_(svc), vm_(vm), state_(state) {}

    void start() {
        if (state_ == "stopped") return;  // the service stays off
        svc_->start();
        if (state_ == "searching") return;
        QTimer::singleShot(200, [this] {
            svc_->simulate_connect("8BitDo Ultimate 2C Wireless");
            svc_->simulate_battery(state_ == "lowbattery" ? 12 : 87, "8BitDo Ultimate 2C Wireless");
            if (state_.empty()) { live_ = true; timer_.start(); return; }
            RawInput in;
            in.abs_x = 200; in.abs_y = 60; in.abs_z = 90; in.abs_rz = 190;
            in.abs_brake = 140; in.abs_gas = 40;
            in.keys = key_bit(kKeySouth) | key_bit(kKeyTl);
            svc_->simulate_input(map_input(in, to_map_config(vm_->settings())));
        });
    }

    void begin_live() {
        timer_.setInterval(33);
        QObject::connect(&timer_, &QTimer::timeout, [this] {
            if (!live_) return;
            seconds_ += 0.033;
            svc_->simulate_input(map_input(ui::demo_input(seconds_), to_map_config(vm_->settings())));
        });
    }

private:
    FakeService* svc_;
    ui::ViewModel* vm_;
    std::string state_;
    QTimer timer_;
    bool live_ = false;
    double seconds_ = 0;
};

int g_signal_pipe[2] = {-1, -1};
void on_signal(int sig) {
    const unsigned char c = static_cast<unsigned char>(sig);
    const ssize_t w = write(g_signal_pipe[1], &c, 1);
    (void)w;
}

}  // namespace

int main(int argc, char** argv) {
    for (int i = 1; i < argc; ++i)
        if (std::strcmp(argv[i], "--screenshot") == 0 && !qEnvironmentVariableIsSet("QT_QPA_PLATFORM")) setenv("QT_QPA_PLATFORM", "offscreen", 1);

    QApplication app(argc, argv);
    app.setQuitOnLastWindowClosed(false);  // hiding to the tray must not end the program
    Args args = parse_args(QCoreApplication::arguments());

    // Only one copy runs: a second start asks the first to show its window and ends (a demo does not take part).
    std::unique_ptr<platform::SingleInstance> guard;
    if (!args.demo) {
        guard = std::make_unique<platform::SingleInstance>(platform::Dirs::from_env().runtime_dir);
        const auto result = guard->acquire();
        if (result == platform::SingleInstance::Result::AlreadyRunning) {
            std::fprintf(stderr, guard->notified() ? "Already running: the open window was asked to show itself.\n" : "Already running.\n");
            return 0;
        }
        if (result == platform::SingleInstance::Result::Error) {
            std::fprintf(stderr, "Could not set up the single instance check (error %d); continuing without it.\n", guard->error());
            guard.reset();
        }
    }
    QApplication::setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
    apply_dark_palette(app);
    const QIcon icon = make_icon();
    app.setWindowIcon(icon);

    // a demo never writes anything
    std::string settings_path = args.settings_path.empty() ? platform::Dirs::from_env().settings_path() : args.settings_path;
    platform::SettingsStore store(settings_path);
    Settings settings = args.demo && args.settings_path.empty() ? Settings{} : store.load().settings;
    if (args.demo_state == "nintendo") settings.nintendo_mode = true;

    if (args.lang_dir.empty()) args.lang_dir = platform::find_data_path("lang");
    if (args.lang_dir.empty()) std::fprintf(stderr, "warning: no translations found (looked for a \"lang\" folder next to the program or in the data folders; --lang-dir sets one)\n");

    // languages that have a file; the saved or detected one; English as the fallback
    std::vector<std::string> languages;
    for (const char* code : ui::kAllLanguages)
        if (QFileInfo::exists(QString::fromStdString(args.lang_dir + "/" + code + ".txt"))) languages.emplace_back(code);
    if (languages.empty()) languages.emplace_back("en");
    const std::string locale = QLocale::system().name().toStdString();
    std::string language = ui::effective_language(settings.language, locale, languages);
    if (!args.language.empty() && std::find(languages.begin(), languages.end(), args.language) != languages.end()) language = args.language;
    const Translations english = load_language(args.lang_dir, "en");

    // a screenshot run has no tray (it would only report a missing one)
    const bool tray_wanted = args.screenshot.empty();
    const bool tray_ok = tray_wanted && uiqt::TrayController::available();
    ui::ViewModel vm(Catalog(english, load_language(args.lang_dir, language)), settings, tray_ok);
    vm.set_active_language(language);

    std::unique_ptr<Service> service;
    FakeService* fake = nullptr;
    if (args.demo) {
        auto f = std::make_unique<FakeService>();
        fake = f.get();
        service = std::move(f);
    } else {
        service = std::make_unique<platform::LinuxService>(std::make_unique<platform::LinuxBackend>(args.paths));
    }
    service->apply_settings(settings);

    // "Start when I log in" is the autostart entry itself; a demo only pretends
    platform::Autostart autostart(platform::Dirs::from_env().autostart_dir(), platform::current_executable_path());
    bool demo_autostart = false;

    std::string os_release = read_file("/etc/os-release");
    if (os_release.empty()) os_release = read_file("/usr/lib/os-release");  // fallback location named by the os-release specification
    ui::Distro distro = ui::detect_distro(os_release);
    if (args.distro == "arch") distro = ui::Distro::Arch;
    else if (args.distro == "debian") distro = ui::Distro::Debian;
    else if (args.distro == "other") distro = ui::Distro::Other;

    uiqt::MainWindow::Hooks hooks;
    uiqt::MainWindow* window_ptr = nullptr;
    hooks.distro = [distro] { return distro; };
    std::unique_ptr<uiqt::TrayController> tray;
    hooks.tray_available = [&] { return tray && uiqt::TrayController::available(); };
    hooks.tooltip_changed = [&](const std::string& text) { if (tray) tray->set_tooltip(QString::fromStdString(text)); };
    hooks.battery_alert = [&](const std::string& title, const std::string& message) {
        if (tray) tray->show_warning(QString::fromStdString(title), QString::fromStdString(message));
    };
    hooks.quit = [] { QApplication::quit(); };
    hooks.settings_changed = [&](const Settings& s) {
        service->apply_settings(s);
        if (!args.demo) store.save(s);  // saved at once
    };
    hooks.autostart_enabled = [&] { return args.demo ? demo_autostart : autostart.is_enabled(); };
    hooks.set_autostart = [&](bool on) {
        if (args.demo) { demo_autostart = on; return true; }
        return autostart.set_enabled(on) == 0;
    };
    hooks.language_selected = [&](const std::string& code) {
        const Settings& s = vm.set_language(code);
        vm.set_catalog(Catalog(english, load_language(args.lang_dir, code)));
        if (!args.demo) store.save(s);
        if (tray) tray->set_texts(QString::fromStdString(vm.catalog().text("TrayOpen")), QString::fromStdString(vm.catalog().text("TrayExit")));
        if (window_ptr) window_ptr->refresh_texts();
    };
    uiqt::MainWindow window(&vm, service.get(), languages, hooks);
    window_ptr = &window;
    uiqt::ServiceBridge bridge(&window);
    service->set_observer(&bridge);

    if (tray_ok) {
        tray = std::make_unique<uiqt::TrayController>(icon, uiqt::TrayController::Callbacks{[&] { window.bring_to_front(); }, [] { QApplication::quit(); }});
        tray->set_texts(QString::fromStdString(vm.catalog().text("TrayOpen")), QString::fromStdString(vm.catalog().text("TrayExit")));
        tray->show();
        window.publish_tooltip();
    }

    // a second copy asks this one to show its window
    std::unique_ptr<QSocketNotifier> guard_notifier;
    if (guard) {
        guard_notifier = std::make_unique<QSocketNotifier>(guard->listen_fd(), QSocketNotifier::Read);
        QObject::connect(guard_notifier.get(), &QSocketNotifier::activated, [&] { if (guard->handle_pending() > 0) window.bring_to_front(); });
    }

    // --minimized hides to the tray, or minimizes when there is no tray
    const ui::StartupAction startup = ui::startup_action(args.minimized, vm.tray_available());
    if (startup == ui::StartupAction::ShowMinimized) window.showMinimized();
    else if (startup == ui::StartupAction::ShowNormal) window.show();

    // problems found by the window itself (not in demo and screenshot runs)
    if (!args.demo && tray_wanted) {
        if (!tray_ok) window.add_ui_notice(ui::UiNotice::NoTrayHost);
        if (QGuiApplication::platformName() == QLatin1String("xcb") && qgetenv("XDG_SESSION_TYPE") == "wayland")
            window.add_ui_notice(ui::UiNotice::WaylandPluginMissing);
    }

    if (args.view == "settings") window.show_settings(true);

    // a notice dialog on request, to check the texts without changing the system
    if (!args.simulate_notice.empty()) {
        if (args.simulate_notice == "access") window.add_ui_notice(ui::UiNotice::NoControllerAccess, "/dev/input/eventNN");
        else if (args.simulate_notice == "uinput") window.add_ui_notice(ui::UiNotice::NoVirtualDevice, "/dev/uinput");
        else if (args.simulate_notice == "bluetooth") window.add_ui_notice(ui::UiNotice::NoBluetoothService);
        else if (args.simulate_notice == "tray") window.add_ui_notice(ui::UiNotice::NoTrayHost);
        else if (args.simulate_notice == "wayland") window.add_ui_notice(ui::UiNotice::WaylandPluginMissing);
        else std::fprintf(stderr, "unknown notice name: %s\n", args.simulate_notice.c_str());
    }

    // for the RAM measurement: signals are handled in the GUI thread through a pipe
    std::unique_ptr<QSocketNotifier> signal_notifier;
    if (args.measure_signals && pipe2(g_signal_pipe, O_CLOEXEC | O_NONBLOCK) == 0) {
        struct sigaction sa{};
        sa.sa_handler = on_signal;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = SA_RESTART;
        sigaction(SIGUSR1, &sa, nullptr);
        sigaction(SIGUSR2, &sa, nullptr);
        signal_notifier = std::make_unique<QSocketNotifier>(g_signal_pipe[0], QSocketNotifier::Read);
        QObject::connect(signal_notifier.get(), &QSocketNotifier::activated, [&] {
            unsigned char buf[16];
            const ssize_t n = read(g_signal_pipe[0], buf, sizeof buf);
            for (ssize_t i = 0; i < n; ++i) {
                if (buf[i] == SIGUSR1) malloc_trim(0);
                else if (buf[i] == SIGUSR2) { if (window.isVisible()) window.hide_to_tray(); else window.bring_to_front(); }
            }
        });
    }

    std::unique_ptr<Demo> demo;
    if (args.demo) {
        demo = std::make_unique<Demo>(fake, &vm, args.demo_state);
        if (args.demo_state.empty()) demo->begin_live();
        demo->start();
    } else if (settings.auto_start_service) {
        service->start();
    }

    if (!args.screenshot.empty()) {
        QTimer::singleShot(900, [&] {
            bool ok = window.grab().save(QString::fromStdString(args.screenshot));
            if (!ok) std::fprintf(stderr, "could not save %s\n", args.screenshot.c_str());
            if (QDialog* dialog = window.findChild<QDialog*>()) {  // a notice dialog is open: save it next to the window shot
                const QString name = QString::fromStdString(args.screenshot);
                dialog->grab().save(name.left(name.lastIndexOf('.')) + "-dialog.png");
            }
            app.exit(ok ? 0 : 1);
        });
    }

    const int rc = app.exec();
    service->set_observer(nullptr);  // no events into a window that is going away
    service->stop();
    return rc;
}
