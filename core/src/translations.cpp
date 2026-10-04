#include "u2c/translations.h"

#include <algorithm>

namespace u2c {

namespace {

std::string_view trim(std::string_view s) {
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) s.remove_prefix(1);
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r')) s.remove_suffix(1);
    return s;
}

bool is_name_char(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
}

}  // namespace

Translations Translations::parse(std::string_view text, std::vector<Warning>* warnings) {
    Translations t;
    if (text.size() >= 3 && text.substr(0, 3) == "\xEF\xBB\xBF") text.remove_prefix(3);
    int line_no = 0;
    while (!text.empty()) {
        const size_t nl = text.find('\n');
        const std::string_view raw = nl == std::string_view::npos ? text : text.substr(0, nl);
        text = nl == std::string_view::npos ? std::string_view{} : text.substr(nl + 1);
        ++line_no;
        const std::string_view line = trim(raw);
        if (line.empty() || line.front() == '#') continue;
        const size_t eq = line.find('=');
        if (eq == std::string_view::npos || trim(line.substr(0, eq)).empty()) {
            if (warnings) warnings->push_back({line_no, std::string(), Warning::Reason::MalformedLine});
            continue;
        }
        const std::string key(trim(line.substr(0, eq)));
        const std::string value(trim(line.substr(eq + 1)));
        if (value.empty() && warnings) warnings->push_back({line_no, key, Warning::Reason::EmptyValue});
        auto it = std::lower_bound(t.entries_.begin(), t.entries_.end(), key,
                                   [](const auto& e, const std::string& k) { return e.first < k; });
        if (it != t.entries_.end() && it->first == key) {
            if (warnings) warnings->push_back({line_no, key, Warning::Reason::DuplicateKey});
            it->second = value;
        } else {
            t.entries_.insert(it, {key, value});
        }
    }
    return t;
}

bool Translations::has(std::string_view key) const {
    auto it = std::lower_bound(entries_.begin(), entries_.end(), key,
                               [](const auto& e, std::string_view k) { return std::string_view(e.first) < k; });
    return it != entries_.end() && it->first == key;
}

std::string_view Translations::get(std::string_view key) const {
    auto it = std::lower_bound(entries_.begin(), entries_.end(), key,
                               [](const auto& e, std::string_view k) { return std::string_view(e.first) < k; });
    if (it != entries_.end() && it->first == key) return it->second;
    return {};
}

std::vector<std::string> Translations::keys() const {
    std::vector<std::string> out;
    out.reserve(entries_.size());
    for (const auto& e : entries_) out.push_back(e.first);
    return out;
}

std::string Catalog::text(std::string_view key) const {
    if (selected_.has(key) && !selected_.get(key).empty()) return std::string(selected_.get(key));
    if (english_.has(key) && !english_.get(key).empty()) return std::string(english_.get(key));
    return "[" + std::string(key) + "]";
}

std::string Catalog::format(std::string_view key,
                            std::initializer_list<std::pair<std::string_view, std::string_view>> values) const {
    const std::string src = text(key);
    std::string out;
    out.reserve(src.size() + 16);
    for (size_t i = 0; i < src.size(); ++i) {
        if (src[i] == '{') {
            const size_t close = src.find('}', i);
            if (close != std::string::npos) {
                const std::string_view name(src.data() + i + 1, close - i - 1);
                bool all_name = !name.empty();
                for (char c : name) all_name = all_name && is_name_char(c);
                if (all_name) {
                    bool found = false;
                    for (const auto& v : values)
                        if (v.first == name) { out.append(v.second); found = true; break; }
                    if (found) { i = close; continue; }
                }
            }
        }
        out += src[i];
    }
    return out;
}

std::vector<std::string> placeholders(std::string_view text) {
    std::vector<std::string> out;
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] != '{') continue;
        const size_t close = text.find('}', i);
        if (close == std::string_view::npos) break;
        const std::string_view name = text.substr(i + 1, close - i - 1);
        bool all_name = !name.empty();
        for (char c : name) all_name = all_name && is_name_char(c);
        if (all_name) { out.emplace_back(name); i = close; }
    }
    std::sort(out.begin(), out.end());
    out.erase(std::unique(out.begin(), out.end()), out.end());
    return out;
}

bool braces_balanced(std::string_view text) {
    bool open = false;
    for (char c : text) {
        if (c == '{') { if (open) return false; open = true; }
        else if (c == '}') { if (!open) return false; open = false; }
    }
    return !open;
}

bool is_valid_utf8(std::string_view s) {
    size_t i = 0;
    while (i < s.size()) {
        const unsigned char c = static_cast<unsigned char>(s[i]);
        size_t n = 0;
        unsigned cp = 0;
        if (c < 0x80) { ++i; continue; }
        else if ((c & 0xE0) == 0xC0) { n = 1; cp = c & 0x1F; }
        else if ((c & 0xF0) == 0xE0) { n = 2; cp = c & 0x0F; }
        else if ((c & 0xF8) == 0xF0) { n = 3; cp = c & 0x07; }
        else return false;
        if (i + n >= s.size()) return false;  // not enough continuation bytes
        for (size_t k = 1; k <= n; ++k) {
            const unsigned char cc = static_cast<unsigned char>(s[i + k]);
            if ((cc & 0xC0) != 0x80) return false;
            cp = (cp << 6) | (cc & 0x3F);
        }
        if ((n == 1 && cp < 0x80) || (n == 2 && cp < 0x800) || (n == 3 && cp < 0x10000)) return false;   // overlong
        if (cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) return false;
        i += n + 1;
    }
    return true;
}

ParityReport check_parity(const Translations& reference, const Translations& other) {
    ParityReport r;
    for (const std::string& k : reference.keys()) {
        if (!other.has(k)) { r.missing.push_back(k); continue; }
        if (placeholders(reference.get(k)) != placeholders(other.get(k))) r.placeholder_mismatch.push_back(k);
        if (other.get(k).empty() || !braces_balanced(other.get(k))) r.empty_or_bad.push_back(k);
    }
    for (const std::string& k : other.keys())
        if (!reference.has(k)) r.extra.push_back(k);
    return r;
}

}  // namespace u2c
