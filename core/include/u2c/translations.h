// UTF-8 key=value language files with {name} placeholders. The English text is the fallback.
#pragma once
#include <initializer_list>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace u2c {

class Translations {
public:
    struct Warning {
        enum class Reason { MalformedLine, DuplicateKey, EmptyValue };
        int line = 0;
        std::string key;
        Reason reason = Reason::MalformedLine;
    };

    // "Key=Value" per line, "#" starts a comment, spaces around key and value are trimmed.
    // A UTF-8 byte order mark and CRLF line endings are accepted. For duplicate keys the last one wins.
    static Translations parse(std::string_view text, std::vector<Warning>* warnings = nullptr);

    bool has(std::string_view key) const;
    std::string_view get(std::string_view key) const;  // empty if the key is missing
    std::vector<std::string> keys() const;
    size_t size() const { return entries_.size(); }

private:
    std::vector<std::pair<std::string, std::string>> entries_;
};

// The texts the program shows: the chosen language, English where a key is missing, "[key]" as the last resort.
class Catalog {
public:
    Catalog(Translations english, Translations selected) : english_(std::move(english)), selected_(std::move(selected)) {}

    std::string text(std::string_view key) const;
    // Replaces {name} with its value. A placeholder without a value stays as it is.
    std::string format(std::string_view key,
                       std::initializer_list<std::pair<std::string_view, std::string_view>> values) const;

private:
    Translations english_, selected_;
};

std::vector<std::string> placeholders(std::string_view text);
bool braces_balanced(std::string_view text);
bool is_valid_utf8(std::string_view text);

// Compares a language with the reference (English): same keys, same placeholders, no empty values.
struct ParityReport {
    std::vector<std::string> missing;  // in the reference only
    std::vector<std::string> extra;  // in the other only
    std::vector<std::string> placeholder_mismatch;  // placeholder names differ
    std::vector<std::string> empty_or_bad;  // empty value or unbalanced braces
    bool ok() const { return missing.empty() && extra.empty() && placeholder_mismatch.empty() && empty_or_bad.empty(); }
};
ParityReport check_parity(const Translations& reference, const Translations& other);

}  // namespace u2c
