// Unit tests for the served web UI asset (host side, env:native).
// Run: pio test -e native
//
// UiHtml.h is pure data (SC-001), so nothing in the firmware ever validates it
// and CI never renders it. These are contract checks on the exact string the
// browser loads - the only automated gate the HTML asset has.
#include <unity.h>

#include <UiHtml.h>

#include <algorithm>
#include <cctype>
#include <cstring>
#include <string>
#include <vector>

namespace {
bool inHtml(const char *needle) {
  return std::strstr(INDEX_HTML, needle) != nullptr;
}
}  // namespace

void test_index_html_has_content(void) {
  TEST_ASSERT_GREATER_THAN_UINT32(1000, static_cast<uint32_t>(strlen(INDEX_HTML)));
}

// A page without lang or charset is unreadable for assistive technology and
// mis-decoded by the browser; both are one-line regressions to lose.
void test_index_html_declares_language_and_charset(void) {
  TEST_ASSERT_TRUE_MESSAGE(inHtml("<html lang="), "missing lang attribute");
  TEST_ASSERT_TRUE_MESSAGE(inHtml("<meta charset=\"utf-8\">"),
                           "missing charset declaration");
}

// #59: with no icon declared the browser requests /favicon.ico on every page
// load and the ESP32 answers 404 (console error on a clean boot). The icon is
// an inline data URI, so it costs no request and no flash partition.
void test_index_html_declares_inline_favicon(void) {
  TEST_ASSERT_TRUE_MESSAGE(inHtml("rel=\"icon\""),
                           "no icon link: browser will request /favicon.ico (#59)");
  TEST_ASSERT_TRUE_MESSAGE(inHtml("data:image/svg+xml,"),
                           "icon must be inline (data URI), not a served file");
}

// Every interactive control is wired by attribute; a renamed or dropped
// attribute silently removes the control from the operator's reach.
void test_index_html_wires_all_controls(void) {
  TEST_ASSERT_TRUE_MESSAGE(inHtml("data-cmd=\"en\""), "driver enable button missing");
  TEST_ASSERT_TRUE_MESSAGE(inHtml("data-cmd=\"off\""), "driver disable button missing");
  TEST_ASSERT_TRUE_MESSAGE(inHtml("data-cmd=\"stop\""), "stop button missing");
  TEST_ASSERT_TRUE_MESSAGE(inHtml("data-jog=\"up\""), "jog up button missing");
  TEST_ASSERT_TRUE_MESSAGE(inHtml("data-jog=\"down\""), "jog down button missing");
  TEST_ASSERT_TRUE_MESSAGE(inHtml("data-prefix=\"feed\""), "feed form missing");
}

// True if the tag containing `anchor` also carries an aria-label (search stops
// at the tag's closing bracket, so a label in a later tag cannot satisfy it).
static bool tagHasAriaLabel(const char *anchor) {
  const char *at = std::strstr(INDEX_HTML, anchor);
  if (at == nullptr) {
    return false;
  }
  const char *tagEnd = std::strchr(at, '>');
  const char *label = std::strstr(at, "aria-label=\"");
  return tagEnd != nullptr && label != nullptr && label < tagEnd;
}

// The jog buttons are labelled with a bare glyph (U+25B2/U+25BC): without a text
// alternative a screen reader announces only "up-pointing triangle", on a
// control that moves the crosshead (WCAG 4.1.2 name/role/value).
void test_jog_buttons_have_text_names(void) {
  TEST_ASSERT_TRUE_MESSAGE(tagHasAriaLabel("data-jog=\"up\""),
                           "jog up button has no aria-label");
  TEST_ASSERT_TRUE_MESSAGE(tagHasAriaLabel("data-jog=\"down\""),
                           "jog down button has no aria-label");
}

// Command replies and connection errors appear in #message; without a live
// region they are silent for assistive technology (WCAG 4.1.3 status messages).
void test_message_area_is_a_live_region(void) {
  TEST_ASSERT_TRUE_MESSAGE(inHtml("id=\"message\" aria-live=\"polite\""),
                           "#message is not announced as a status message");
}

// Keyboard parity for the hold-to-jog buttons (WCAG 2.1.1): the pointer path
// alone (pointerdown/pointerup) is unreachable from the keyboard, so a keyboard
// user cannot jog at all - and keydown/keyup plus blur is what gives the same
// release-stops-it and focus-lost-stops-it behaviour.
void test_jog_supports_keyboard_activation(void) {
  TEST_ASSERT_TRUE_MESSAGE(inHtml("addEventListener(\"keydown\""), "no keydown handler");
  TEST_ASSERT_TRUE_MESSAGE(inHtml("addEventListener(\"keyup\""), "no keyup handler");
  TEST_ASSERT_TRUE_MESSAGE(inHtml("addEventListener(\"blur\""),
                           "no blur handler: a held key would never stop");
}

// --- M2-T005: embedded locale dictionary (plan D3, FR-005) -------------------

// Extract every occurrence of `prefix + "..."` (key up to the next quote) from
// [from, to) into a sorted unique vector. With keysOnly, a token counts only
// when the closing quote is immediately followed by ':' (JS object key), so
// dictionary VALUES do not pollute the key set.
static std::vector<std::string> extractKeys(const char *from, const char *to,
                                            const std::string &prefix,
                                            bool keysOnly = false,
                                            bool wordStart = false) {
  std::vector<std::string> keys;
  for (const char *p = from; p != nullptr && p < to;) {
    p = std::strstr(p, prefix.c_str());
    if (p == nullptr || p >= to) {
      break;
    }
    const bool boundaryOk =
        !wordStart || (p > INDEX_HTML && !std::isalnum(p[-1]) && p[-1] != '_' &&
                       p[-1] != '$' && p[-1] != '.');
    p += prefix.size();
    const char *end = std::strchr(p, '"');
    if (end != nullptr && end < to) {
      const bool isKey = boundaryOk && (!keysOnly || (end + 1 < to && end[1] == ':'));
      if (isKey) {
        keys.emplace_back(p, end);
      }
      p = end;
    }
  }
  std::sort(keys.begin(), keys.end());
  keys.erase(std::unique(keys.begin(), keys.end()), keys.end());
  return keys;
}

static const char *dictStart() {
  return std::strstr(INDEX_HTML, "const I18N = {");
}

// Static markup default: English (SC-001 fresh boot). The visible part (up to
// the script block) must carry no German umlauts and no German UI words -
// the DE strings may only live inside the JS dictionary.
void test_static_markup_is_english(void) {
  const char *script = std::strstr(INDEX_HTML, "<script>");
  TEST_ASSERT_NOT_NULL(script);
  for (const char *p = INDEX_HTML; p < script; ++p) {
    TEST_ASSERT_TRUE_MESSAGE(static_cast<unsigned char>(*p) < 0x80,
                             "non-ASCII byte in static markup (DE leaked)");
  }
  const std::vector<std::string> german = {"Selbsttest", "Treiber", "Vorschub",
                                           "Steuerung", "setzen", "fahren",
                                           "Waegezelle", "Gewicht"};
  const std::string head(INDEX_HTML, script);
  for (const auto &word : german) {
    TEST_ASSERT_TRUE_MESSAGE(head.find(word) == std::string::npos,
                             ("German word in static markup: " + word).c_str());
  }
}

// The dictionary exists for both locales and the html default is lang="en".
void test_i18n_dictionary_has_both_locales(void) {
  TEST_ASSERT_TRUE_MESSAGE(inHtml("<html lang=\"en\">"),
                           "static default must be English (SC-001)");
  const char *dict = dictStart();
  TEST_ASSERT_NOT_NULL_MESSAGE(dict, "I18N dictionary missing");
  TEST_ASSERT_NOT_NULL(std::strstr(dict, "en: {"));
  TEST_ASSERT_NOT_NULL(std::strstr(dict, "de: {"));
}

// Every key the static markup asks for (text, placeholder, aria-label) and
// every dynamic t("...") key must exist in BOTH dictionaries - a missing DE
// entry silently serves English (and the EN fallback hides a missing EN key).
void test_i18n_dictionary_is_complete(void) {
  const char *dict = dictStart();
  TEST_ASSERT_NOT_NULL(dict);
  const char *en = std::strstr(dict, "en: {");
  const char *de = std::strstr(dict, "de: {");
  TEST_ASSERT_NOT_NULL(en);
  TEST_ASSERT_NOT_NULL(de);
  const std::vector<std::string> enKeys = extractKeys(en, de, "\"", true);
  const char *dictEnd = std::strstr(de, "\n    };");
  TEST_ASSERT_NOT_NULL(dictEnd);
  const std::vector<std::string> deKeys = extractKeys(de, dictEnd, "\"", true);
  TEST_ASSERT_TRUE_MESSAGE(enKeys == deKeys, "EN and DE dictionaries differ");
  TEST_ASSERT_TRUE_MESSAGE(enKeys.size() > 40, "dictionary unexpectedly small");

  std::vector<std::string> used;
  for (const char *attr :
       {"data-i18n=\"", "data-i18n-ph=\"", "data-i18n-al=\""}) {
    auto keys = extractKeys(INDEX_HTML, dict, attr);
    used.insert(used.end(), keys.begin(), keys.end());
  }
  auto dynamic =
      extractKeys(dictEnd, INDEX_HTML + strlen(INDEX_HTML), "t(\"", false,
                  true);  // real t(" calls only, not createElement("li")
  used.insert(used.end(), dynamic.begin(), dynamic.end());
  // Indirect keys: the selftest status map feeds t() through its values.
  const std::vector<std::string> indirect = {"stOk", "stWarn", "stFail",
                                             "stNoConn", "stNoWired"};
  used.insert(used.end(), indirect.begin(), indirect.end());
  std::sort(used.begin(), used.end());
  used.erase(std::unique(used.begin(), used.end()), used.end());
  for (const auto &key : used) {
    TEST_ASSERT_TRUE_MESSAGE(
        std::binary_search(enKeys.begin(), enKeys.end(), key),
        ("static/dynamic key missing in EN dict: " + key).c_str());
    TEST_ASSERT_TRUE_MESSAGE(
        std::binary_search(deKeys.begin(), deKeys.end(), key),
        ("static/dynamic key missing in DE dict: " + key).c_str());
  }
}

// FR-003 switch path: the selector exists and routes through the shared
// `lang` command the serial CLI uses (single policy).
void test_language_selector_present(void) {
  TEST_ASSERT_TRUE_MESSAGE(inHtml("data-lang=\"en\""), "EN selector missing");
  TEST_ASSERT_TRUE_MESSAGE(inHtml("data-lang=\"de\""), "DE selector missing");
  TEST_ASSERT_TRUE_MESSAGE(inHtml("sendCommand(\"lang \" + btn.dataset.lang)"),
                           "selector does not send the lang command");
  TEST_ASSERT_TRUE_MESSAGE(inHtml("applyLocale(data.locale)"),
                           "UI does not follow the server locale");
}

void runUiHtmlTests(void) {
  RUN_TEST(test_index_html_has_content);
  RUN_TEST(test_index_html_declares_language_and_charset);
  RUN_TEST(test_index_html_declares_inline_favicon);
  RUN_TEST(test_index_html_wires_all_controls);
  RUN_TEST(test_jog_buttons_have_text_names);
  RUN_TEST(test_message_area_is_a_live_region);
  RUN_TEST(test_jog_supports_keyboard_activation);
  RUN_TEST(test_static_markup_is_english);
  RUN_TEST(test_i18n_dictionary_has_both_locales);
  RUN_TEST(test_i18n_dictionary_is_complete);
  RUN_TEST(test_language_selector_present);
}
