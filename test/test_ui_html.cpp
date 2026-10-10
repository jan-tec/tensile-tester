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

void runUiHtmlTests(void) {
  RUN_TEST(test_index_html_has_content);
  RUN_TEST(test_index_html_declares_language_and_charset);
  RUN_TEST(test_index_html_declares_inline_favicon);
  RUN_TEST(test_index_html_wires_all_controls);
}
