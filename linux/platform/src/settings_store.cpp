#include "u2c/platform/settings_store.h"

#include <cerrno>

#include "u2c/platform/paths.h"

namespace u2c::platform {

SettingsStore::LoadResult SettingsStore::load() const {
    LoadResult r;
    std::string text;
    const int st = read_file_limited(path_, kMaxFileBytes, text);
    if (st == ENOENT) return r;
    r.file_existed = true;
    if (st == E2BIG) { r.too_large = true; return r; }
    if (st != 0) { r.read_error = st; return r; }
    ParseResult pr = parse_settings(text);
    r.settings = pr.settings;
    r.warnings = std::move(pr.warnings);
    return r;
}

int SettingsStore::save(const Settings& settings) const {
    const size_t slash = path_.rfind('/');
    if (slash == std::string::npos || slash == 0) return EINVAL;
    int err = 0;
    if (!make_dirs(path_.substr(0, slash), 0700, &err)) return err ? err : EIO;
    std::string existing;
    if (read_file_limited(path_, kMaxFileBytes, existing) != 0) existing.clear();  // missing, too large or unreadable: start fresh
    return write_file_atomic(path_, serialize_settings(settings, existing), 0600);
}

}  // namespace u2c::platform
