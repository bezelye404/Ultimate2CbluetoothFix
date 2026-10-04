#include "u2c/platform/paths.h"

#include <fcntl.h>
#include <pwd.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace u2c::platform {

namespace {
bool absolute_env(const char* name, std::string& out) {
    const char* v = std::getenv(name);
    if (v && v[0] == '/') { out = v; return true; }
    return false;
}
}  // namespace

Dirs Dirs::from_env() {
    Dirs d;
    if (!absolute_env("HOME", d.home)) {
        const passwd* pw = getpwuid(getuid());
        if (pw && pw->pw_dir && pw->pw_dir[0] == '/') d.home = pw->pw_dir;
    }
    if (!absolute_env("XDG_CONFIG_HOME", d.config_home)) d.config_home = d.home + "/.config";
    absolute_env("XDG_RUNTIME_DIR", d.runtime_dir);
    while (d.config_home.size() > 1 && d.config_home.back() == '/') d.config_home.pop_back();
    while (d.runtime_dir.size() > 1 && d.runtime_dir.back() == '/') d.runtime_dir.pop_back();
    return d;
}

std::string current_executable_path() {
    char buf[4096];
    const ssize_t n = readlink("/proc/self/exe", buf, sizeof buf - 1);
    if (n <= 0) return {};
    buf[n] = '\0';
    return buf;
}

namespace {
bool exists(const std::string& path) {
    struct stat st{};
    return stat(path.c_str(), &st) == 0;
}
}  // namespace

std::string find_data_path(const std::string& name, const std::string& executable_path,
                           const std::string& data_home, const std::string& data_dirs) {
    std::vector<std::string> candidates;
    const size_t slash = executable_path.rfind('/');
    if (slash != std::string::npos) {
        const std::string exe_dir = executable_path.substr(0, slash);
        candidates.push_back(exe_dir + "/data/" + name);
        candidates.push_back(exe_dir + "/../share/ultimate2cfixer/" + name);
    }
    if (!data_home.empty() && data_home[0] == '/') candidates.push_back(data_home + "/ultimate2cfixer/" + name);
    size_t pos = 0;
    while (pos <= data_dirs.size()) {
        const size_t colon = data_dirs.find(':', pos);
        std::string dir = data_dirs.substr(pos, colon == std::string::npos ? std::string::npos : colon - pos);
        while (dir.size() > 1 && dir.back() == '/') dir.pop_back();
        if (!dir.empty() && dir[0] == '/') candidates.push_back(dir + "/ultimate2cfixer/" + name);
        if (colon == std::string::npos) break;
        pos = colon + 1;
    }
    for (const std::string& c : candidates)
        if (exists(c)) return c;
    return {};
}

std::string find_data_path(const std::string& name) {
    std::string data_home, data_dirs;
    if (!absolute_env("XDG_DATA_HOME", data_home)) data_home = Dirs::from_env().home + "/.local/share";
    const char* dirs = std::getenv("XDG_DATA_DIRS");
    data_dirs = dirs && dirs[0] ? dirs : "/usr/local/share:/usr/share";
    return find_data_path(name, current_executable_path(), data_home, data_dirs);
}

bool make_dirs(const std::string& path, mode_t mode, int* err) {
    if (path.empty() || path[0] != '/') { if (err) *err = EINVAL; return false; }
    std::string current;
    size_t pos = 1;
    while (pos <= path.size()) {
        const size_t slash = path.find('/', pos);
        const size_t end = slash == std::string::npos ? path.size() : slash;
        current = path.substr(0, end);
        if (!current.empty() && current != "/") {
            if (mkdir(current.c_str(), mode) != 0 && errno != EEXIST) { if (err) *err = errno; return false; }
        }
        if (slash == std::string::npos) break;
        pos = slash + 1;
    }
    struct stat st{};
    if (stat(path.c_str(), &st) != 0 || !S_ISDIR(st.st_mode)) { if (err) *err = ENOTDIR; return false; }
    if (err) *err = 0;
    return true;
}

int read_file_limited(const std::string& path, size_t max_bytes, std::string& out) {
    out.clear();
    const int fd = open(path.c_str(), O_RDONLY | O_CLOEXEC);
    if (fd < 0) return errno;
    char buf[4096];
    for (;;) {
        const ssize_t n = read(fd, buf, sizeof buf);
        if (n < 0) {
            if (errno == EINTR) continue;
            const int e = errno;
            close(fd);
            out.clear();
            return e;
        }
        if (n == 0) break;
        if (out.size() + static_cast<size_t>(n) > max_bytes) { close(fd); out.clear(); return E2BIG; }
        out.append(buf, static_cast<size_t>(n));
    }
    close(fd);
    return 0;
}

int write_file_atomic(const std::string& path, const std::string& content, mode_t mode) {
    const size_t slash = path.rfind('/');
    if (slash == std::string::npos) return EINVAL;
    const std::string tmp = path + ".tmp." + std::to_string(getpid());
    const int fd = open(tmp.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, mode);
    if (fd < 0) return errno;
    int err = 0;
    size_t done = 0;
    while (done < content.size()) {
        const ssize_t n = write(fd, content.data() + done, content.size() - done);
        if (n < 0) { if (errno == EINTR) continue; err = errno; break; }
        done += static_cast<size_t>(n);
    }
    if (err == 0 && fsync(fd) != 0) err = errno;
    if (close(fd) != 0 && err == 0) err = errno;
    if (err == 0 && fchmodat(AT_FDCWD, tmp.c_str(), mode, 0) != 0) err = errno;  // the creation mode is reduced by the umask
    if (err == 0 && rename(tmp.c_str(), path.c_str()) != 0) err = errno;
    if (err != 0) unlink(tmp.c_str());
    return err;
}

}  // namespace u2c::platform
