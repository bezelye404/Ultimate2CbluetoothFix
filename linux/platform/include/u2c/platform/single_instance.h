// Only one copy runs. The first copy holds a lock file and listens on a Unix socket in the runtime directory;
// a second copy asks it to show its window and exits. No D-Bus needed.
#pragma once
#include <string>

struct sockaddr_un;

namespace u2c::platform {

class SingleInstance {
public:
    enum class Result {
        First,  // this process is first; listen_fd() is valid
        AlreadyRunning,  // another instance exists; notified() says whether it was asked to show its window
        Error,  // the runtime directory or socket could not be set up; error() has the errno
    };

    // normally $XDG_RUNTIME_DIR; if empty, a private /tmp/ultimate2cfixer-<uid> (mode 0700) is used
    explicit SingleInstance(std::string runtime_dir, std::string name = "ultimate2cfixer")
        : runtime_dir_(std::move(runtime_dir)), name_(std::move(name)) {}
    ~SingleInstance();
    SingleInstance(const SingleInstance&) = delete;
    SingleInstance& operator=(const SingleInstance&) = delete;

    Result acquire();
    bool notified() const { return notified_; }
    int error() const { return error_; }

    // First instance only: a non-blocking socket for the event loop; call handle_pending() when it is readable.
    int listen_fd() const { return listen_fd_; }
    // Accepts waiting connections; returns how many valid "show" requests arrived (same user only).
    int handle_pending();

    std::string socket_path() const { return dir_ + "/" + name_ + ".sock"; }
    std::string lock_path() const { return dir_ + "/" + name_ + ".lock"; }

private:
    bool prepare_dir();
    bool notify_first_instance();
    // The plain path, or /proc/self/fd/<dir>/<name> when the plain path is too long for sockaddr_un.
    bool socket_address(struct sockaddr_un& addr) const;
    std::string socket_name() const { return name_ + ".sock"; }

    std::string runtime_dir_, name_, dir_;
    int dir_fd_ = -1;
    int lock_fd_ = -1;
    int listen_fd_ = -1;
    bool notified_ = false;
    int error_ = 0;
};

}  // namespace u2c::platform
