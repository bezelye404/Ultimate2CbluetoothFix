#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>

#include "check.h"
#include "u2c/translations.h"

using namespace u2c;

namespace {

std::string read_file(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    std::stringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

// CTest sets U2C_RESOURCE_DIR. Run from the repository folder without it, the default below is used.
std::string resource_dir() {
    const char* dir = std::getenv("U2C_RESOURCE_DIR");
    return dir && dir[0] ? dir : "core/resources";
}

void test_parse() {
    std::vector<Translations::Warning> w;
    Translations t = Translations::parse("\xEF\xBB\xBF# comment\r\nHello = Merhaba \r\n\r\nBye=Güle güle\nHello=Selam\nbroken line\nEmpty=\n", &w);
    CHECK(t.has("Hello"));
    CHECK(t.get("Hello") == "Selam");
    CHECK(t.get("Bye") == "Güle güle");
    CHECK(t.has("Empty") && t.get("Empty").empty());
    CHECK(!t.has("Nothing"));
    CHECK(t.get("Nothing").empty());
    CHECK_EQ(t.size(), 3);
    bool dup = false, malformed = false, empty = false;
    for (const auto& x : w) {
        dup = dup || x.reason == Translations::Warning::Reason::DuplicateKey;
        malformed = malformed || x.reason == Translations::Warning::Reason::MalformedLine;
        empty = empty || x.reason == Translations::Warning::Reason::EmptyValue;
    }
    CHECK(dup && malformed && empty);
    const auto keys = t.keys();
    CHECK(keys == std::vector<std::string>({"Bye", "Empty", "Hello"}));
    CHECK(Translations::parse("").size() == 0);
}

void test_placeholders_and_format() {
    CHECK(placeholders("Battery is at {level}%. {name} {level}") == std::vector<std::string>({"level", "name"}));
    CHECK(placeholders("no placeholder, 50% {} { } {a b}").empty());
    CHECK(braces_balanced("a {b} c"));
    CHECK(!braces_balanced("a {b c"));
    CHECK(!braces_balanced("a b} c"));
    CHECK(!braces_balanced("{{x}}"));

    Catalog c(Translations::parse("Msg=Battery is at {level}%.\nOnlyEn=English only\nTwo={a}-{b}-{a}\n"),
              Translations::parse("Msg=Pil seviyesi %{level}.\nTwo={b}/{a}\n"));
    CHECK(c.format("Msg", {{"level", "15"}}) == "Pil seviyesi %15.");
    CHECK(c.text("OnlyEn") == "English only");
    CHECK(c.text("Missing") == "[Missing]");
    CHECK(c.format("Two", {{"a", "1"}, {"b", "2"}}) == "2/1");
    CHECK(c.format("Msg", {}) == "Pil seviyesi %{level}.");  // a missing value leaves the placeholder visible
    CHECK(c.format("Msg", {{"other", "x"}}) == "Pil seviyesi %{level}.");
}

void test_utf8() {
    CHECK(is_valid_utf8("plain ascii"));
    CHECK(is_valid_utf8("Türkçe ğüşiöç İĞÜŞÖÇ • español ñ¿¡"));
    CHECK(is_valid_utf8("\xF0\x9F\x8E\xAE"));  // 4-byte character
    CHECK(!is_valid_utf8("\xC3"));
    CHECK(!is_valid_utf8("\xC0\x80"));
    CHECK(!is_valid_utf8("\xED\xA0\x80"));
    CHECK(!is_valid_utf8("\xFF"));
    CHECK(!is_valid_utf8("a\x80z"));  // stray continuation byte
}

void test_parity_function() {
    Translations en = Translations::parse("A=one {x}\nB=two\nC=three\n");
    Translations ok = Translations::parse("A=uno {x}\nB=dos\nC=tres\n");
    CHECK(check_parity(en, ok).ok());
    Translations bad = Translations::parse("A=uno {y}\nC=\nD=extra\n");
    const ParityReport r = check_parity(en, bad);
    CHECK(!r.ok());
    CHECK(r.missing == std::vector<std::string>({"B"}));
    CHECK(r.extra == std::vector<std::string>({"D"}));
    CHECK(r.placeholder_mismatch == std::vector<std::string>({"A"}));
    CHECK(r.empty_or_bad == std::vector<std::string>({"C"}));
}

// Number of characters (not bytes) of a UTF-8 text.
size_t utf8_length(std::string_view s) {
    size_t n = 0;
    for (unsigned char c : s) if ((c & 0xC0) != 0x80) ++n;
    return n;
}

// The real language files: every file in lang/ is checked against English.
void test_real_files() {
    const char* const languages[] = {"tr", "es"};
    const std::string en_text = read_file(resource_dir() + "/lang/en.txt");
    CHECK(!en_text.empty());
    CHECK(is_valid_utf8(en_text));
    std::vector<Translations::Warning> we;
    const Translations en = Translations::parse(en_text, &we);
    CHECK(we.empty());
    CHECK(en.size() >= 40);

    // texts that must fit a fixed place in the window (card and button widths, in characters)
    struct Limit { const char* key; size_t max_chars; };
    const Limit limits[] = {
        {"StatusTitle", 22}, {"BatteryTitle", 12}, {"LiveInputTitle", 20}, {"NoDevice", 20}, {"StartBtn", 18}, {"StopBtn", 18},
        {"ClearBtn", 8}, {"SettingsBack", 12}, {"StatusConnected", 22}, {"BatteryLow", 34}, {"BatteryModerate", 34}, {"BatteryHealthy", 34},
        {"MinimizeOnClose", 70}, {"AutoStartService", 70}, {"LowBatteryNotification", 70}, {"ExclusiveGrab", 70}, {"StartWithSystem", 70},
    };
    std::vector<std::pair<std::string, const Translations*>> all = {{"en", &en}};
    std::vector<Translations> loaded;
    loaded.reserve(2);
    for (const char* lang : languages) {
        const std::string text = read_file(resource_dir() + "/lang/" + lang + ".txt");
        CHECK(!text.empty());
        CHECK(is_valid_utf8(text));
        std::vector<Translations::Warning> w;
        loaded.push_back(Translations::parse(text, &w));
        CHECK(w.empty());
        all.push_back({lang, &loaded.back()});
    }
    for (const auto& [lang, t] : all) {
        CHECK_EQ(en.size(), t->size());
        const ParityReport r = check_parity(en, *t);
        for (const auto& k : r.missing) std::printf("  missing in %s: %s\n", lang.c_str(), k.c_str());
        for (const auto& k : r.extra) std::printf("  extra in %s: %s\n", lang.c_str(), k.c_str());
        for (const auto& k : r.placeholder_mismatch) std::printf("  placeholder mismatch in %s: %s\n", lang.c_str(), k.c_str());
        for (const auto& k : r.empty_or_bad) std::printf("  empty or bad in %s: %s\n", lang.c_str(), k.c_str());
        CHECK(r.ok());

        // leftovers of old problems: \x escapes, printf formats, doubled percent
        for (const auto& k : t->keys()) {
            const std::string v(t->get(k));
            CHECK(v.find("\\x") == std::string::npos);
            CHECK(v.find("%d") == std::string::npos);
            CHECK(v.find("%%") == std::string::npos);
            CHECK(v.find("\\n") == std::string::npos);
        }
        for (const Limit& l : limits) {
            const size_t n = utf8_length(t->get(l.key));
            if (n > l.max_chars) std::printf("  too long in %s: %s (%zu characters, at most %zu)\n", lang.c_str(), l.key, n, l.max_chars);
            CHECK(n <= l.max_chars);
        }
        // the three cycling buttons: "label: value" must fit the 340 px buttons (about 40 characters)
        const Catalog c(en, *t);
        for (const char* v : {"DeadzoneOff", "DeadzoneLow", "DeadzoneNormal", "DeadzoneHigh"})
            CHECK(utf8_length(c.text("DeadzoneLabel") + ": " + c.text(v)) <= 40);
        for (const char* v : {"CurveLinear", "CurveSmooth", "CurveAggressive"})
            CHECK(utf8_length(c.text("CurveLabel") + ": " + c.text(v)) <= 40);
        CHECK(utf8_length(c.text("PollingRateLabel") + ": 1000 Hz") <= 40);
    }

    const Translations& tr = *all[1].second;
    const Translations& es = *all[2].second;
    CHECK(tr.get("LowBatteryNotification") == "Düşük pil bildirimi (<=%15)");
    CHECK(en.get("DeadzoneNormal") == "Normal (12%)");
    CHECK(tr.get("DeadzoneNormal") == "Normal (%12)");
    CHECK(es.get("DeadzoneNormal") == "Normal (12%)");
    CHECK(en.get("StartWithSystem") == "Start when I log in");
    CHECK(es.get("ClearBtn") == "Limpiar");
    const Catalog c(en, tr);
    CHECK(c.format("LowBatteryAlertMsg", {{"level", "15"}}) == "Pil seviyesi %15. Lütfen kolu şarj edin.");
    CHECK(c.format("LogBatteryLevel", {{"name", "Pad"}, {"level", "80"}}) == "Pad Pil: %80");
    const Catalog ce(en, en);
    CHECK(ce.format("LowBatteryAlertMsg", {{"level", "15"}}) == "Battery is at 15%. Please charge your controller.");
    const Catalog cs(en, es);
    CHECK(cs.format("LowBatteryAlertMsg", {{"level", "15"}}) == "La batería está al 15%. Carga tu control.");
    CHECK(cs.format("LogControllerConnected", {{"name", "Pad"}}) == "Pad conectado.");
}

}  // namespace

void test_translations() {
    test_parse();
    test_placeholders_and_format();
    test_utf8();
    test_parity_function();
    test_real_files();
}
