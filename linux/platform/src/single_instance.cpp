#include "u2c/platform/single_instance.h"

#include <fcntl.h>
#include <poll.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>

#include "u2c/platform/paths.h"

namespace u2c::platform {

namespace {

bool fill_address(const std::string& path, sockaddr_un& addr) {
    if (path.size() >= sizeof addr.sun_path) return false;
    std::memset(&addr, 0, sizeof addr);
    addr.sun_family = AF_UNIX;
    std::memcpy(addr.sun_path, path.c_str(), path.size() + 1);
    return true;
}

}  // namespace

SingleInstance::~SingleInstance() {
    if (listen_fd_ >= 0) {
        close(listen_fd_);
        if (dir_fd_ >= 0) unlinkat(dir_fd_, socket_name().c_str(), 0);  // we held the lock, so the socket is ours
    }
    if (lock_fd_ >= 0) close(lock_fd_);
    if (dir_fd_ >= 0) close(dir_fd_);
}

bool SingleInstance::socket_address(sockaddr_un& addr) const {
    if (fill_address(socket_path(), addr)) return true;
    // The plain path is longer than sockaddr_un allows (108 bytes): go through the open directory.
    return dir_fd_ >= 0 && fill_address("/proc/self/fd/" + std::to_string(dir_fd_) + "/" + socket_name(), addr);
}

bool SingleInstance::prepare_dir() {
    dir_ = runtime_dir_;
    bool created_fallback = false;
    if (dir_.empty()) {
        dir_ = "/tmp/ultimate2cfixer-" + std::to_string(getuid());
        created_fallback = true;
    }
    if (created_fallback && mkdir(dir_.c_str(), 0700) != 0 && errno != EEXIST) { error_ = errno; return false; }
    struct stat st{};
    if (lstat(dir_.c_str(), &st) != 0) { error_ = errno; return false; }
    // Must be a real directory of this user that nobody else can enter (like XDG_RUNTIME_DIR, mode 0700).
    if (!S_ISDIR(st.st_mode) || st.st_uid != getuid() || (st.st_mode & 077) != 0) { error_ = EACCES; return false; }
    dir_fd_ = open(dir_.c_str(), O_PATH | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
    if (dir_fd_ < 0) { error_ = errno; return false; }
    return true;
}

bool SingleInstance::notify_first_instance() {
    sockaddr_un addr;
    if (!socket_address(addr)) { error_ = ENAMETOOLONG; return false; }
    // The first instance may still be setting up its socket: retry for about a second.
    for (int attempt = 0; attempt < 20; ++attempt) {
        const int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
        if (fd < 0) { error_ = errno; return false; }
        if (connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof addr) == 0) {
            const char msg[] = "show\n";
            bool ok = send(fd, msg, sizeof msg - 1, MSG_NOSIGNAL) == static_cast<ssize_t>(sizeof msg - 1);
            if (ok) {
                pollfd p{fd, POLLIN, 0};
                char reply[8] = {};
                ok = poll(&p, 1, 1000) > 0 && read(fd, reply, sizeof reply - 1) > 0 && std::strncmp(reply, "ok", 2) == 0;
            }
            close(fd);
            return ok;
        }
        close(fd);
        usleep(50 * 1000);
    }
    error_ = ECONNREFUSED;
    return false;
}

SingleInstance::Result SingleInstance::acquire() {
    if (lock_fd_ >= 0) return listen_fd_ >= 0 ? Result::First : Result::AlreadyRunning;
    if (!prepare_dir()) return Result::Error;

    lock_fd_ = openat(dir_fd_, (name_ + ".lock").c_str(), O_RDWR | O_CREAT | O_CLOEXEC | O_NOFOLLOW, 0600);
    if (lock_fd_ < 0) { error_ = errno; return Result::Error; }
    if (flock(lock_fd_, LOCK_EX | LOCK_NB) != 0) {
        if (errno != EWOULDBLOCK) { error_ = errno; close(lock_fd_); lock_fd_ = -1; return Result::Error; }
        close(lock_fd_);
        lock_fd_ = -1;
        notified_ = notify_first_instance();
        return Result::AlreadyRunning;
    }

    // First instance: (re)create the socket. A leftover file is stale, because we hold the lock.
    sockaddr_un addr;
    if (!socket_address(addr)) { error_ = ENAMETOOLONG; close(lock_fd_); lock_fd_ = -1; return Result::Error; }
    unlinkat(dir_fd_, socket_name().c_str(), 0);
    listen_fd_ = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if (listen_fd_ < 0) { error_ = errno; close(lock_fd_); lock_fd_ = -1; return Result::Error; }
    const mode_t old_umask = umask(0177);
    const int b = bind(listen_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof addr);
    const int bind_errno = errno;
    umask(old_umask);
    if (b != 0 || listen(listen_fd_, 4) != 0) {
        error_ = b != 0 ? bind_errno : errno;
        close(listen_fd_); listen_fd_ = -1;
        close(lock_fd_); lock_fd_ = -1;
        return Result::Error;
    }
    return Result::First;
}

int SingleInstance::handle_pending() {
    if (listen_fd_ < 0) return 0;
    int shows = 0;
    for (;;) {
        const int fd = accept4(listen_fd_, nullptr, nullptr, SOCK_CLOEXEC | SOCK_NONBLOCK);
        if (fd < 0) break;  // EAGAIN: nothing more waiting
        ucred cred{};
        socklen_t len = sizeof cred;
        const bool same_user = getsockopt(fd, SOL_SOCKET, SO_PEERCRED, &cred, &len) == 0 && cred.uid == getuid();
        char buf[64] = {};
        pollfd p{fd, POLLIN, 0};
        if (same_user && poll(&p, 1, 200) > 0) {
            const ssize_t n = read(fd, buf, sizeof buf - 1);
            if (n == 5 && std::memcmp(buf, "show\n", 5) == 0) {
                ++shows;
                // MSG_NOSIGNAL: a client that already went away must not kill the program with SIGPIPE
                const ssize_t w = send(fd, "ok\n", 3, MSG_NOSIGNAL);
                (void)w;
            }
        }
        close(fd);
    }
    return shows;
}

}  // namespace u2c::platform
