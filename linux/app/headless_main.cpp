// Headless runner: the Linux service without a window, for development and tests.
//   ultimate2cfixer-headless [--settings <file>] [--duration <seconds>] [--lang en|tr|es] [--lang-dir <dir>]
//                            [--input-dir <dir>] [--uinput <path>]   run the service with the real controller
//   ultimate2cfixer-headless --diagnose [--duration N]                 same, then print how many events came per code and
//                                                                     how many changes per report field (counts only)
//   ultimate2cfixer-headless --probe                                  only look for the controller (nothing is opened for
//                                                                     writing, grabbed or created)
//   ultimate2cfixer-headless --demo                                   drive the fake service
// It prints status, notices and log lines. Input values (buttons, sticks) are never printed.
#include <signal.h>
#include <unistd.h>

#include <chrono>
#include <map>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>

#include "u2c/fake_service.h"
#include "u2c/platform/linux_backend.h"
#include "u2c/platform/linux_service.h"
#include "u2c/platform/paths.h"
#include "u2c/platform/settings_store.h"
#include "u2c/translations.h"


using namespace u2c;
using namespace u2c::platform;

namespace {

std::string read_all(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    std::stringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

// --diagnose: counts what the controller sends per event type and code, never the values.
struct Counts {
    std::mutex m;
    std::map<std::pair<int, int>, long> events;  // (type, code) -> number of events
    long syn_reports = 0;
    long changes[7] = {};  // lx, ly, rx, ry, left trigger, right trigger, buttons
};

class CountingSource : public InputSource {
public:
    CountingSource(std::unique_ptr<InputSource> inner, Counts* counts) : inner_(std::move(inner)), counts_(counts) {}
    int fd() const override { return inner_->fd(); }
    std::string name() const override { return inner_->name(); }
    int read_events(InputEvent* buf, int capacity) override {
        const int n = inner_->read_events(buf, capacity);
        if (n > 0) {
            std::lock_guard<std::mutex> lock(counts_->m);
            for (int i = 0; i < n; ++i) {
                if (buf[i].type == 0 && buf[i].code == 0) ++counts_->syn_reports;
                else ++counts_->events[{buf[i].type, buf[i].code}];
            }
        }
        return n;
    }
    bool read_state(std::vector<InputEvent>& out) override { return inner_->read_state(out); }
    bool set_grab(bool on) override { return inner_->set_grab(on); }
private:
    std::unique_ptr<InputSource> inner_;
    Counts* counts_;
};

class CountingBackend : public Backend {
public:
    CountingBackend(std::unique_ptr<Backend> inner, Counts* counts) : inner_(std::move(inner)), counts_(counts) {}
    int watch_fd() override { return inner_->watch_fd(); }
    void drain_watch() override { inner_->drain_watch(); }
    std::vector<Candidate> scan(std::vector<Notice>& problems) override { return inner_->scan(problems); }
    std::unique_ptr<InputSource> open(const Candidate& c, int* err) override {
        auto s = inner_->open(c, err);
        return s ? std::make_unique<CountingSource>(std::move(s), counts_) : nullptr;
    }
    std::unique_ptr<PadSink> create_pad(int* err) override { return inner_->create_pad(err); }
    std::unique_ptr<BatteryProvider> create_battery_provider() override { return inner_->create_battery_provider(); }
private:
    std::unique_ptr<Backend> inner_;
    Counts* counts_;
};

class Printer : public ServiceObserver {
public:
    explicit Printer(const Catalog& catalog, Counts* counts = nullptr) : catalog_(catalog), counts_(counts) {}
    void on_status(ServiceStatus s, const std::string& device) override {
        switch (s) {
            case ServiceStatus::Searching: line("status", catalog_.text("StatusSearching")); break;
            case ServiceStatus::Connected: line("status", catalog_.text("StatusConnected") + " (" + device + ")"); break;
            case ServiceStatus::Disconnected: line("status", catalog_.text("WaitingReconnect")); break;
            case ServiceStatus::Stopped: line("status", catalog_.text("StatusStopped")); break;
        }
    }
    void on_battery(const BatteryInfo& b) override {
        line("battery", b.level >= 0 ? std::to_string(b.level) + "%" : std::string("--"));
    }
    void on_input(const PadReport& r) override {
        ++inputs_;  // counted only, values are never printed
        if (counts_) {
            std::lock_guard<std::mutex> lock(counts_->m);
            const long diffs[7] = {r.lx != prev_.lx, r.ly != prev_.ly, r.rx != prev_.rx, r.ry != prev_.ry,
                                   r.left_trigger != prev_.left_trigger, r.right_trigger != prev_.right_trigger, r.buttons != prev_.buttons};
            for (int i = 0; i < 7; ++i) counts_->changes[i] += diffs[i];
            prev_ = r;
        }
    }
    void on_log(const LogEvent& e) override {
        std::string text = catalog_.text(e.key);
        for (const auto& v : e.values) {
            const std::string token = "{" + v.first + "}";
            for (size_t pos = text.find(token); pos != std::string::npos; pos = text.find(token, pos + v.second.size()))
                text.replace(pos, token.size(), v.second);
        }
        line("log", text);
    }
    void on_notice(const Notice& n) override {
        const char* key = n.kind == NoticeKind::NoControllerAccess ? "NoticeNoControllerAccess"
                        : n.kind == NoticeKind::NoVirtualDevice ? "NoticeNoVirtualDevice" : "NoticeNoBluetoothService";
        std::string text = catalog_.text(key);
        const std::string token = "{detail}";
        const size_t pos = text.find(token);
        if (pos != std::string::npos) text.replace(pos, token.size(), n.detail);
        line("notice", text);
    }
    long inputs() const { return inputs_.load(); }

private:
    void line(const char* kind, const std::string& text) {
        std::lock_guard<std::mutex> lock(m_);
        std::printf("[%s] %s\n", kind, text.c_str());
        std::fflush(stdout);
    }
    const Catalog& catalog_;
    Counts* counts_;
    PadReport prev_;
    std::mutex m_;
    std::atomic<long> inputs_{0};
};

int run_probe(const LinuxBackendPaths& paths) {
    LinuxBackend backend(paths);
    std::vector<Notice> problems;
    const auto found = backend.scan(problems);
    for (const auto& n : problems)
        std::printf("problem: no permission to read %s\n", n.detail.c_str());
    if (found.empty()) std::printf("no matching controller found\n");
    for (const auto& c : found) {
        int err = 0;
        auto src = backend.open(c, &err);  // opened read-only, nothing is grabbed
        std::printf("found: %s  name: %s\n", c.path.c_str(), src ? src->name().c_str() : "(could not open)");
    }
    // battery (a read-only question to BlueZ over D-Bus)
    if (auto battery = backend.create_battery_provider()) {
        battery->start();
        if (!battery->available()) std::printf("battery: Bluetooth service not reachable\n");
        else if (battery->current().level < 0) std::printf("battery: no controller battery reported by BlueZ\n");
        else std::printf("battery: %d%% (%s)\n", battery->current().level, battery->current().device.c_str());
    } else {
        std::printf("battery: this build has no battery support (libsystemd was not available)\n");
    }
    return found.empty() ? 1 : 0;
}

int run_demo(Printer& printer, int seconds) {
    FakeService svc;
    svc.set_observer(&printer);
    svc.start();
    svc.simulate_connect("8BitDo Ultimate 2C Wireless (demo)");
    svc.simulate_battery(87, "8BitDo Ultimate 2C Wireless (demo)");
    for (int i = 0; i < seconds; ++i) std::this_thread::sleep_for(std::chrono::milliseconds(100));
    svc.simulate_disconnect();
    svc.stop();
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    std::string settings_path, lang = "en", lang_dir;
    LinuxBackendPaths paths;
    int duration = 0;
    bool probe = false, demo = false, diagnose = false;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        auto value = [&](std::string& out) { if (i + 1 < argc) out = argv[++i]; };
        if (a == "--probe") probe = true;
        else if (a == "--demo") demo = true;
        else if (a == "--diagnose") diagnose = true;
        else if (a == "--settings") value(settings_path);
        else if (a == "--lang") value(lang);
        else if (a == "--lang-dir") value(lang_dir);
        else if (a == "--input-dir") value(paths.input_dir);
        else if (a == "--uinput") value(paths.uinput);
        else if (a == "--duration") { std::string d; value(d); duration = std::atoi(d.c_str()); }
        else { std::fprintf(stderr, "unknown option: %s\n", a.c_str()); return 2; }
    }
    if (probe) return run_probe(paths);
    if (lang_dir.empty()) lang_dir = u2c::platform::find_data_path("lang");

    const Translations english = Translations::parse(read_all(lang_dir + "/en.txt"));
    const Translations chosen = lang == "en" ? english : Translations::parse(read_all(lang_dir + "/" + lang + ".txt"));
    if (english.size() == 0) std::fprintf(stderr, "warning: no translations found (looked for a \"lang\" folder next to the program or in the data folders; --lang-dir sets one)\n");
    const Catalog catalog(english, chosen);
    Counts counts;
    Printer printer(catalog, diagnose ? &counts : nullptr);

    if (demo) return run_demo(printer, duration > 0 ? duration * 10 : 20);

    if (settings_path.empty()) settings_path = Dirs::from_env().settings_path();
    const SettingsStore store(settings_path);
    const auto loaded = store.load();
    std::printf("settings: %s (%s, %zu warning(s))\n", settings_path.c_str(), loaded.file_existed ? "read" : "not found, defaults",
                loaded.warnings.size());
    if (loaded.too_large) std::printf("settings file is too large and was ignored\n");

    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGINT);
    sigaddset(&set, SIGTERM);
    pthread_sigmask(SIG_BLOCK, &set, nullptr);  // before the service threads exist, so they all inherit the mask
    signal(SIGPIPE, SIG_IGN);

    std::unique_ptr<Backend> backend = std::make_unique<LinuxBackend>(paths);
    if (diagnose) backend = std::make_unique<CountingBackend>(std::move(backend), &counts);
    LinuxService service(std::move(backend));
    service.set_observer(&printer);
    service.apply_settings(loaded.settings);
    service.start();

    timespec timeout{1, 0};
    int waited = 0;
    for (;;) {
        const int sig = sigtimedwait(&set, nullptr, &timeout);
        if (sig == SIGINT || sig == SIGTERM) break;
        if (duration > 0 && ++waited >= duration) break;
    }
    service.stop();
    std::printf("input updates written: %ld\n", printer.inputs());
    if (diagnose) {
        std::lock_guard<std::mutex> lock(counts.m);
        static const char* const field[7] = {"left stick X", "left stick Y", "right stick X", "right stick Y", "left trigger", "right trigger", "buttons/D-pad"};
        std::printf("diagnose: events from the controller (counts only):\n");
        if (counts.events.empty() && counts.syn_reports == 0) std::printf("  none\n");
        for (const auto& e : counts.events) std::printf("  type %d code 0x%02x: %ld\n", e.first.first, e.first.second, e.second);
        std::printf("  SYN_REPORT frames: %ld\n", counts.syn_reports);
        std::printf("diagnose: changes of each report field between written updates:\n");
        for (int i = 0; i < 7; ++i) std::printf("  %-14s %ld\n", field[i], counts.changes[i]);
    }
    return 0;
}
