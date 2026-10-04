// Tests for the file helpers, settings store, autostart and single instance.
// Everything happens in temporary directories; the real configuration is never touched.
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

#include <atomic>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <thread>

#include "../../../core/tests/check.h"
#include "u2c/platform/autostart.h"
#include "u2c/platform/paths.h"
#include "u2c/platform/settings_store.h"
#include "u2c/platform/single_instance.h"

using namespace u2c;
using namespace u2c::platform;
namespace fs = std::filesystem;

void test_evdev_logic();
void test_linux_service();
void test_battery();

namespace {

struct TempDir {
    std::string path;
    TempDir() {
        std::string tmpl = (fs::temp_directory_path() / "u2c-test-XXXXXX").string();
        char* p = mkdtemp(tmpl.data());
        path = p ? p : "";
    }
    ~TempDir() {
        if (path.empty()) return;
        std::error_code ec;
        fs::permissions(path, fs::perms::owner_all, fs::perm_options::add, ec);  // in case a test made it read-only
        fs::remove_all(path, ec);
    }
};

struct EnvGuard {  // sets or removes an environment variable and restores it at the end
    std::string name, old;
    bool had;
    EnvGuard(const char* n, const char* value) : name(n) {
        const char* o = std::getenv(n);
        had = o != nullptr;
        if (o) old = o;
        if (value) setenv(n, value, 1); else unsetenv(n);
    }
    ~EnvGuard() { if (had) setenv(name.c_str(), old.c_str(), 1); else unsetenv(name.c_str()); }
};

// connect to a socket in `dir` also when the full path is longer than sockaddr_un allows
int connect_unix(const std::string& dir, const std::string& name) {
    const int dfd = open(dir.c_str(), O_PATH | O_DIRECTORY | O_CLOEXEC);
    const int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    const std::string p = "/proc/self/fd/" + std::to_string(dfd) + "/" + name;
    std::strncpy(addr.sun_path, p.c_str(), sizeof addr.sun_path - 1);
    const int rc = connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof addr);
    close(dfd);
    if (rc != 0) { close(fd); return -1; }
    return fd;
}

std::string slurp(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}
void spit(const std::string& path, const std::string& text) {
    std::ofstream out(path, std::ios::binary);
    out << text;
}
mode_t mode_of(const std::string& path) {
    struct stat st{};
    return stat(path.c_str(), &st) == 0 ? (st.st_mode & 0777) : 0;
}
int count_entries(const std::string& dir) {
    int n = 0;
    for (auto it = fs::directory_iterator(dir); it != fs::directory_iterator(); ++it) ++n;
    return n;
}

void test_dirs() {
    {
        EnvGuard h("HOME", "/home/someone"), c("XDG_CONFIG_HOME", "/custom/cfg/"), r("XDG_RUNTIME_DIR", "/run/user/7");
        const Dirs d = Dirs::from_env();
        CHECK(d.home == "/home/someone");
        CHECK(d.config_home == "/custom/cfg");
        CHECK(d.runtime_dir == "/run/user/7");
        CHECK(d.settings_path() == "/custom/cfg/ultimate2cfixer/settings.conf");
        CHECK(d.autostart_dir() == "/custom/cfg/autostart");
    }
    {
        EnvGuard h("HOME", "/home/someone"), c("XDG_CONFIG_HOME", "relative/path"), r("XDG_RUNTIME_DIR", "also/relative");
        const Dirs d = Dirs::from_env();
        CHECK(d.config_home == "/home/someone/.config");  // relative values are ignored (XDG rule)
        CHECK(d.runtime_dir.empty());
    }
    {
        EnvGuard h("HOME", "/home/someone"), c("XDG_CONFIG_HOME", nullptr), r("XDG_RUNTIME_DIR", nullptr);
        const Dirs d = Dirs::from_env();
        CHECK(d.config_home == "/home/someone/.config");
    }
    CHECK(!current_executable_path().empty());
    CHECK(current_executable_path()[0] == '/');
}

void test_find_data_path() {
    TempDir t;
    const std::string exe = t.path + "/prefix/bin/ultimate2cfixer";
    const std::string build_data = t.path + "/prefix/bin/data/lang";
    const std::string share_data = t.path + "/prefix/share/ultimate2cfixer/lang";
    const std::string home_data = t.path + "/home/ultimate2cfixer/lang";
    const std::string sys_a = t.path + "/sysa", sys_b = t.path + "/sysb";
    const std::string dirs = sys_a + ":" + sys_b + "/";

    CHECK(make_dirs(t.path + "/prefix/bin", 0700));
    CHECK(find_data_path("lang", exe, t.path + "/home", dirs).empty());
    CHECK(make_dirs(sys_b + "/ultimate2cfixer/lang", 0700));
    CHECK(find_data_path("lang", exe, t.path + "/home", dirs) == sys_b + "/ultimate2cfixer/lang");  // last data dir, trailing slash dropped
    CHECK(make_dirs(sys_a + "/ultimate2cfixer/lang", 0700));
    CHECK(find_data_path("lang", exe, t.path + "/home", dirs) == sys_a + "/ultimate2cfixer/lang");   // earlier entry wins
    CHECK(make_dirs(home_data, 0700));
    CHECK(find_data_path("lang", exe, t.path + "/home", dirs) == home_data);  // data home before data dirs
    CHECK(make_dirs(share_data, 0700));
    CHECK(find_data_path("lang", exe, t.path + "/home", dirs) == t.path + "/prefix/bin/../share/ultimate2cfixer/lang");
    CHECK(make_dirs(build_data, 0700));
    CHECK(find_data_path("lang", exe, t.path + "/home", dirs) == build_data);                      // next to the program wins
    CHECK(find_data_path("other", exe, t.path + "/home", dirs).empty());
    CHECK(find_data_path("lang", "", "relative/home", "relative:/nonexistent-dir").empty());  // relative entries are ignored
    CHECK(find_data_path("lang", "", "", "").empty());  // no executable path, nothing configured
}

void test_files() {
    TempDir t;
    CHECK(!t.path.empty());
    int err = -1;
    CHECK(make_dirs(t.path + "/a/b/c", 0700, &err));
    CHECK_EQ(err, 0);
    CHECK_EQ(mode_of(t.path + "/a"), 0700);
    CHECK_EQ(mode_of(t.path + "/a/b/c"), 0700);
    CHECK(make_dirs(t.path + "/a/b/c", 0700));
    spit(t.path + "/file", "x");
    CHECK(!make_dirs(t.path + "/file", 0700, &err));  // a file is in the way
    CHECK(!make_dirs("relative", 0700, &err));
    CHECK_EQ(err, EINVAL);

    std::string out;
    CHECK_EQ(read_file_limited(t.path + "/missing", 100, out), ENOENT);
    spit(t.path + "/five", "12345");
    CHECK_EQ(read_file_limited(t.path + "/five", 5, out), 0);  // exactly the limit is fine
    CHECK(out == "12345");
    CHECK_EQ(read_file_limited(t.path + "/five", 4, out), E2BIG);  // one byte more is not
    CHECK(out.empty());
    CHECK(read_file_limited(t.path + "/a", 100, out) != 0);  // a directory is not a file

    std::string binary;
    for (int i = 0; i < 5000; ++i) binary += static_cast<char>(i % 256);
    CHECK_EQ(write_file_atomic(t.path + "/bin", binary, 0600), 0);
    CHECK_EQ(read_file_limited(t.path + "/bin", 100000, out), 0);
    CHECK(out == binary);
    CHECK_EQ(mode_of(t.path + "/bin"), 0600);
    CHECK_EQ(write_file_atomic(t.path + "/bin", "new", 0600), 0);
    CHECK(slurp(t.path + "/bin") == "new");
    CHECK_EQ(write_file_atomic(t.path + "/nodir/file", "x", 0600), ENOENT);
    CHECK_EQ(write_file_atomic("nofolder", "x", 0600), EINVAL);

    // the target is a directory: the final rename fails after the temporary file exists and must be cleaned up
    fs::create_directory(t.path + "/isdir");
    CHECK(write_file_atomic(t.path + "/isdir", "x", 0600) != 0);
    CHECK_EQ(count_entries(t.path + "/isdir"), 0);
    {
        int leftovers = 0;
        for (auto it = fs::directory_iterator(t.path); it != fs::directory_iterator(); ++it)
            if (it->path().filename().string().find(".tmp.") != std::string::npos) ++leftovers;
        CHECK_EQ(leftovers, 0);
    }

    // a read-only directory: the write fails, the old file stays, no temporary file is left
    fs::create_directory(t.path + "/ro");
    spit(t.path + "/ro/keep", "old");
    chmod((t.path + "/ro").c_str(), 0500);
    if (geteuid() != 0) {
        CHECK(write_file_atomic(t.path + "/ro/keep", "new", 0600) != 0);
        CHECK(slurp(t.path + "/ro/keep") == "old");
        CHECK_EQ(count_entries(t.path + "/ro"), 1);
    }
    chmod((t.path + "/ro").c_str(), 0700);
}

void test_settings_store() {
    TempDir t;
    const std::string path = t.path + "/cfg/ultimate2cfixer/settings.conf";
    SettingsStore store(path);

    SettingsStore::LoadResult r = store.load();
    CHECK(!r.file_existed);
    CHECK(r.settings == Settings{});
    CHECK(r.warnings.empty());

    Settings s;
    s.deadzone = 3; s.nintendo_mode = true; s.exclusive_grab = false; s.language = "es";
    CHECK_EQ(store.save(s), 0);
    CHECK_EQ(mode_of(t.path + "/cfg/ultimate2cfixer"), 0700);
    CHECK_EQ(mode_of(path), 0600);
    r = store.load();
    CHECK(r.file_existed);
    CHECK(r.settings == s);
    CHECK(r.warnings.empty());

    // user comments and unknown keys survive a save
    spit(path, "# mine\nNote=keep\nDeadzone=1\n");
    s.deadzone = 2;
    CHECK_EQ(store.save(s), 0);
    const std::string text = slurp(path);
    CHECK(text.find("# mine\n") == 0);
    CHECK(text.find("Note=keep\n") != std::string::npos);
    CHECK(text.find("Deadzone=2\n") != std::string::npos);
    CHECK(store.load().settings == s);
    CHECK_EQ(count_entries(t.path + "/cfg/ultimate2cfixer"), 1);

    spit(path, std::string(SettingsStore::kMaxFileBytes + 1, 'x'));
    r = store.load();
    CHECK(r.too_large);
    CHECK(r.settings == Settings{});
    CHECK_EQ(store.save(s), 0);  // replaces the oversized file with a normal one
    CHECK(store.load().settings == s);

    spit(path, "Deadzone=99\nnonsense\nLanguage=fr\n");
    r = store.load();
    CHECK_EQ(r.warnings.size(), 3);
    CHECK(r.settings == Settings{});

    // the path is a directory: a read error is reported, defaults are used
    SettingsStore dir_store(t.path + "/cfg");
    r = dir_store.load();
    CHECK(r.read_error != 0);
    CHECK(r.settings == Settings{});

    if (geteuid() != 0) {
        spit(path, "Deadzone=1\n");
        chmod((t.path + "/cfg/ultimate2cfixer").c_str(), 0500);
        CHECK(store.save(s) != 0);
        CHECK(slurp(path) == "Deadzone=1\n");
        chmod((t.path + "/cfg/ultimate2cfixer").c_str(), 0700);
    }
    CHECK(SettingsStore("relative.conf").save(s) != 0);
}

void test_autostart() {
    CHECK(Autostart::quote_exec_argument("/usr/bin/app") == "/usr/bin/app");
    CHECK(Autostart::quote_exec_argument("/opt/my app/run") == "\"/opt/my app/run\"");
    CHECK(Autostart::quote_exec_argument("/a\"b") == "\"/a\\\"b\"");
    CHECK(Autostart::quote_exec_argument("/a$b") == "\"/a\\$b\"");
    CHECK(Autostart::quote_exec_argument("/a`b") == "\"/a\\`b\"");
    CHECK(Autostart::quote_exec_argument("/a\\b") == "\"/a\\\\b\"");
    CHECK(Autostart::quote_exec_argument("/100%/x") == "\"/100%%/x\"");
    CHECK(Autostart::desktop_entry("/usr/bin/app").find("Exec=/usr/bin/app --minimized\n") != std::string::npos);
    CHECK(Autostart::desktop_entry("/a b/app").find("Exec=\"/a b/app\" --minimized\n") != std::string::npos);
    CHECK(Autostart::desktop_entry("/x").rfind("[Desktop Entry]\n", 0) == 0);

    TempDir t;
    Autostart a(t.path + "/cfg/autostart", "/opt/my app/ultimate2cfixer");
    CHECK(!a.is_enabled());
    CHECK_EQ(a.set_enabled(false), 0);  // disabling something that does not exist is fine
    CHECK_EQ(a.set_enabled(true), 0);
    CHECK(a.is_enabled());
    CHECK_EQ(mode_of(t.path + "/cfg/autostart"), 0700);
    CHECK_EQ(mode_of(a.file_path()), 0644);
    const std::string text = slurp(a.file_path());
    CHECK(text.find("Exec=\"/opt/my app/ultimate2cfixer\" --minimized\n") != std::string::npos);
    CHECK(text.find("Type=Application\n") != std::string::npos);
    CHECK_EQ(a.set_enabled(true), 0);
    CHECK(a.is_enabled());

    spit(a.file_path(), "[Desktop Entry]\r\nType=Application\r\nHidden=true\r\n");  // disabled by hand
    CHECK(!a.is_enabled());
    spit(a.file_path(), "garbage without a section");
    CHECK(!a.is_enabled());
    CHECK_EQ(a.set_enabled(true), 0);
    CHECK(a.is_enabled());
    CHECK_EQ(a.set_enabled(false), 0);
    CHECK(!a.is_enabled());
    CHECK(!fs::exists(a.file_path()));
    CHECK_EQ(count_entries(t.path + "/cfg/autostart"), 0);

    CHECK_EQ(Autostart(t.path + "/x", "relative/app").set_enabled(true), EINVAL);
    CHECK_EQ(Autostart(t.path + "/x", "").set_enabled(true), EINVAL);
}

void test_single_instance() {
    TempDir t;
    EnvGuard dummy("U2C_DUMMY", nullptr);
    chmod(t.path.c_str(), 0700);

    {
        SingleInstance first(t.path, "u2c-test");
        CHECK(first.acquire() == SingleInstance::Result::First);
        CHECK(first.listen_fd() >= 0);
        CHECK_EQ(mode_of(first.socket_path()), 0600);
        CHECK(first.acquire() == SingleInstance::Result::First);

        // second and third start: each asks the first to show its window, then reports AlreadyRunning
        int shows = 0;
        for (int n = 0; n < 3; ++n) {
            std::atomic<bool> done{false};
            SingleInstance::Result res = SingleInstance::Result::Error;
            bool notified = false;
            std::thread th([&] {
                SingleInstance other(t.path, "u2c-test");
                res = other.acquire();
                notified = other.notified();
                done = true;
            });
            while (!done) {
                shows += first.handle_pending();
                usleep(2000);
            }
            th.join();
            CHECK(res == SingleInstance::Result::AlreadyRunning);
            CHECK(notified);
        }
        CHECK_EQ(shows, 3);
        CHECK_EQ(first.handle_pending(), 0);

        // a client that sends garbage or nothing is ignored and does not break the listener
        for (const char* junk : {"hello\n", "show", "show\nshow\n", ""}) {
            const int fd = connect_unix(t.path, "u2c-test.sock");
            CHECK(fd >= 0);
            if (fd < 0) continue;
            if (*junk) { const ssize_t w = write(fd, junk, std::strlen(junk)); (void)w; }
            if (*junk == 's' && std::strlen(junk) == 4) close(fd);  // "show" without newline: closed early
            usleep(5000);
            CHECK_EQ(first.handle_pending(), 0);
            if (!(*junk == 's' && std::strlen(junk) == 4)) close(fd);
        }

        // a client that asks and leaves at once: the request still counts and the program must survive
        // (replying to a closed socket would raise SIGPIPE without MSG_NOSIGNAL)
        {
            const int fd = connect_unix(t.path, "u2c-test.sock");
            CHECK(fd >= 0);
            if (fd >= 0) {
                const ssize_t w = write(fd, "show\n", 5);
                (void)w;
                close(fd);
                usleep(5000);
                CHECK_EQ(first.handle_pending(), 1);
            }
        }
    }

    CHECK(!fs::exists(t.path + "/u2c-test.sock"));
    {
        SingleInstance again(t.path, "u2c-test");
        CHECK(again.acquire() == SingleInstance::Result::First);
    }

    {
        const int fd = socket(AF_UNIX, SOCK_STREAM, 0);
        sockaddr_un addr{};
        addr.sun_family = AF_UNIX;
        const std::string sp = t.path + "/u2c-test.sock";
        const int dfd = open(t.path.c_str(), O_PATH | O_DIRECTORY | O_CLOEXEC);
        const std::string via = "/proc/self/fd/" + std::to_string(dfd) + "/u2c-test.sock";
        std::strncpy(addr.sun_path, via.c_str(), sizeof addr.sun_path - 1);
        CHECK(bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof addr) == 0);
        close(dfd);
        close(fd);  // the file stays, nobody listens
        CHECK(fs::exists(sp));
        SingleInstance fresh(t.path, "u2c-test");
        CHECK(fresh.acquire() == SingleInstance::Result::First);
    }

    // a lock held but nobody listening (a dead slow first instance): AlreadyRunning, not notified
    {
        SingleInstance holder(t.path, "u2c-test");
        CHECK(holder.acquire() == SingleInstance::Result::First);
        close(holder.listen_fd());  // simulate a first instance that never answers
        unlink(holder.socket_path().c_str());
        SingleInstance late(t.path, "u2c-test");
        CHECK(late.acquire() == SingleInstance::Result::AlreadyRunning);
        CHECK(!late.notified());
        CHECK_EQ(late.error(), ECONNREFUSED);
    }

    {
        TempDir open_dir;
        chmod(open_dir.path.c_str(), 0755);  // others could enter: refused
        SingleInstance s(open_dir.path, "u2c-test");
        CHECK(s.acquire() == SingleInstance::Result::Error);
        CHECK_EQ(s.error(), EACCES);
        SingleInstance missing(t.path + "/does-not-exist", "u2c-test");
        CHECK(missing.acquire() == SingleInstance::Result::Error);
        SingleInstance toolong(t.path, std::string(200, 'n'));  // socket path longer than sockaddr_un allows
        CHECK(toolong.acquire() == SingleInstance::Result::Error);
        CHECK_EQ(toolong.error(), ENAMETOOLONG);
    }
}

}  // namespace

int main() {
    test_dirs();
    test_find_data_path();
    test_files();
    test_settings_store();
    test_autostart();
    test_single_instance();
    test_evdev_logic();
    test_linux_service();
    test_battery();
    std::printf("checks: %ld, failures: %ld\n", u2ctest::g_checks, u2ctest::g_failures);
    return u2ctest::g_failures == 0 ? 0 : 1;
}

void test_settings() {}
void test_translations() {}
void test_rate_limiter() {}
