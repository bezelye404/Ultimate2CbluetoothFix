// Reads and writes the settings file. The text format is in core (settings.h); this class does the file work.
#pragma once
#include <string>
#include <vector>

#include "u2c/settings.h"

namespace u2c::platform {

class SettingsStore {
public:
    static constexpr size_t kMaxFileBytes = 64 * 1024;

    explicit SettingsStore(std::string path) : path_(std::move(path)) {}
    const std::string& path() const { return path_; }

    struct LoadResult {
        Settings settings;  // defaults when the file is missing or unusable
        std::vector<SettingsWarning> warnings;
        bool file_existed = false;
        bool too_large = false;  // bigger than kMaxFileBytes: ignored completely
        int read_error = 0;  // errno of a read failure other than "missing"
    };
    LoadResult load() const;

    // Keeps comments and unknown keys of the existing file (if it is readable and small).
    // Creates the directory with mode 0700. Returns 0 or an errno value.
    int save(const Settings& settings) const;

private:
    std::string path_;
};

}  // namespace u2c::platform
