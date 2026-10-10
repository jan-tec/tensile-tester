// Unit tests for the served web UI asset (host side, env:native).
// Run: pio test -e native
//
// UiHtml.h is pure data (SC-001), so nothing in the firmware ever validates it
// and CI never renders it. These are contract checks on the exact string the
// browser loads - the only automated gate the HTML asset has.
#include <unity.h>

#include <UiHtml.h>

#include <cstring>

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

void runUiHtmlTests(void) {
  RUN_TEST(test_index_html_has_content);
  RUN_TEST(test_index_html_declares_language_and_charset);
  RUN_TEST(test_index_html_declares_inline_favicon);
  RUN_TEST(test_index_html_wires_all_controls);
  RUN_TEST(test_jog_buttons_have_text_names);
  RUN_TEST(test_message_area_is_a_live_region);
  RUN_TEST(test_jog_supports_keyboard_activation);
}
