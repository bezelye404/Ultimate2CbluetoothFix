// "Start when I log in": an XDG autostart entry in the user's autostart directory.
// The file is the source of truth: enabled means it exists and is not Hidden.
#pragma once
#include <string>

namespace u2c::platform {

class Autostart {
public:
    // exec_path: absolute path of the program; the entry runs it with --minimized.
    Autostart(std::string autostart_dir, std::string exec_path)
        : dir_(std::move(autostart_dir)), exec_(std::move(exec_path)) {}

    bool is_enabled() const;
    int set_enabled(bool enabled);  // 0 or an errno value; disabling a missing file is not an error
    std::string file_path() const { return dir_ + "/ultimate2cfixer.desktop"; }

    // Content of the entry, and the Desktop Entry quoting rules for the Exec value.
    static std::string desktop_entry(const std::string& exec_path);
    static std::string quote_exec_argument(const std::string& arg);

private:
    std::string dir_;
    std::string exec_;
};

}  // namespace u2c::platform
