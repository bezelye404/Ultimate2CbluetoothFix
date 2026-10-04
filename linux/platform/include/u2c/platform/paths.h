// Where files live (XDG base directories) and small file helpers. Qt-free, POSIX only.
#pragma once
#include <sys/types.h>

#include <string>

namespace u2c::platform {

struct Dirs {
    std::string home;
    std::string config_home;  // $XDG_CONFIG_HOME if absolute, else $HOME/.config
    std::string runtime_dir;  // $XDG_RUNTIME_DIR if absolute, else empty (single instance then uses a private fallback)

    static Dirs from_env();
    std::string app_config_dir() const { return config_home + "/ultimate2cfixer"; }
    std::string settings_path() const { return app_config_dir() + "/settings.conf"; }
    std::string autostart_dir() const { return config_home + "/autostart"; }
};

// Absolute path of the running executable, empty on failure.
std::string current_executable_path();

// Finds shipped data (a file or folder such as "lang" or "logo.ico"). The first of these that exists wins:
// <executable folder>/data/<name> (build folder), <executable folder>/../share/ultimate2cfixer/<name> (installed),
// then <data_home>/ultimate2cfixer/<name> and each entry of data_dirs with the same suffix. Empty if none exists.
std::string find_data_path(const std::string& name, const std::string& executable_path,
                           const std::string& data_home, const std::string& data_dirs);

// The same for the running process: its executable and $XDG_DATA_HOME / $XDG_DATA_DIRS with the standard defaults.
std::string find_data_path(const std::string& name);

// Creates the directory and missing parents; created directories get `mode`. True if the directory exists afterwards.
// errno goes to *err if given.
bool make_dirs(const std::string& path, mode_t mode, int* err = nullptr);

// Reads a whole file, at most `max_bytes`. Returns 0 ok, ENOENT missing, E2BIG too large, else errno.
int read_file_limited(const std::string& path, size_t max_bytes, std::string& out);

// Writes the file atomically: temporary file in the same directory, fsync, rename.
// Returns 0 or an errno value; the temporary file is removed on failure.
int write_file_atomic(const std::string& path, const std::string& content, mode_t mode);

}  // namespace u2c::platform
