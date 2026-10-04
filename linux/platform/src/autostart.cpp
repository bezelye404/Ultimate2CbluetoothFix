#include "u2c/platform/autostart.h"

#include <cerrno>
#include <unistd.h>

#include "u2c/platform/paths.h"

namespace u2c::platform {

std::string Autostart::quote_exec_argument(const std::string& arg) {
    // Desktop Entry spec: an argument with special characters goes in double quotes; inside them " ` $ and \ get a
    // backslash and a literal % is written as %%.
    static const std::string special = " \t\n\"'\\><~|&;$*?#()`";
    if (arg.find_first_of(special) == std::string::npos && arg.find('%') == std::string::npos) return arg;
    std::string out = "\"";
    for (char c : arg) {
        if (c == '"' || c == '`' || c == '$' || c == '\\') out += '\\';
        out += c;
        if (c == '%') out += '%';
    }
    out += '"';
    return out;
}

std::string Autostart::desktop_entry(const std::string& exec_path) {
    std::string s;
    s += "[Desktop Entry]\n";
    s += "Type=Application\n";
    s += "Name=Ultimate2CFixer\n";
    s += "Comment=Starts the Ultimate2CFixer controller service when you log in\n";
    s += "Exec=" + quote_exec_argument(exec_path) + " --minimized\n";
    s += "Terminal=false\n";
    s += "X-GNOME-Autostart-enabled=true\n";
    return s;
}

bool Autostart::is_enabled() const {
    std::string text;
    if (read_file_limited(file_path(), 16 * 1024, text) != 0) return false;
    // Hidden=true means disabled (a user may do that by hand)
    size_t pos = 0;
    while (pos < text.size()) {
        const size_t nl = text.find('\n', pos);
        std::string line = text.substr(pos, nl == std::string::npos ? std::string::npos : nl - pos);
        pos = nl == std::string::npos ? text.size() : nl + 1;
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();
        if (line == "Hidden=true") return false;
    }
    return text.find("[Desktop Entry]") != std::string::npos;
}

int Autostart::set_enabled(bool enabled) {
    if (!enabled) {
        if (unlink(file_path().c_str()) != 0 && errno != ENOENT) return errno;
        return 0;
    }
    if (exec_.empty() || exec_[0] != '/') return EINVAL;
    int err = 0;
    if (!make_dirs(dir_, 0700, &err)) return err ? err : EIO;
    return write_file_atomic(file_path(), desktop_entry(exec_), 0644);
}

}  // namespace u2c::platform
