// Unit tests for the M2 message-string table (host side, env:native).
// Run: pio test -e native
//
// M2 TC-002 (T002, RED-first): every Msg id must carry a non-empty EN and a
// non-empty DE entry, so a half-translated table cannot ship. The table lives
// in src/Text.h as a header-inline array of {en, de} rows generated from the
// same X-macro list as the enum (#88): id/EN/DE are paired by construction,
// the runtime walk below is the second net. The host test links the header
// directly (src/*.cpp is not part of the native test build, same pattern as
// the pure ErrorCode.h). PROGMEM is a no-op on both the host and ESP32
// (unified address space), so plain indexing is valid everywhere.
#include <unity.h>

#include <Text.h>

#include <cstddef>
#include <cstdint>
#include <cstring>

void test_msg_count_is_positive(void) {
  TEST_ASSERT_GREATER_THAN_INT(0, static_cast<int>(Msg::Count));
}

void test_every_msg_has_en_entry(void) {
  for (int i = 0; i < static_cast<int>(Msg::Count); ++i) {
    const char *entry = msgEn(static_cast<Msg>(i));
    TEST_ASSERT_NOT_NULL_MESSAGE(entry, "Msg id has no EN entry");
    TEST_ASSERT_TRUE_MESSAGE(entry[0] != '\0', "Msg id has an empty EN entry");
  }
}

void test_every_msg_has_de_entry(void) {
  for (int i = 0; i < static_cast<int>(Msg::Count); ++i) {
    const char *entry = msgDe(static_cast<Msg>(i));
    TEST_ASSERT_NOT_NULL_MESSAGE(entry, "Msg id has no DE entry");
    TEST_ASSERT_TRUE_MESSAGE(entry[0] != '\0', "Msg id has an empty DE entry");
  }
}

// The row table and the enum must cover the same id range - one row per id,
// index N == Msg id N (enforced by the shared X-macro list; asserted here at
// runtime as the TC-002 complement to the compile-time static_assert).
void test_table_covers_msg_exactly(void) {
  TEST_ASSERT_EQUAL_UINT32(static_cast<uint32_t>(Msg::Count),
                           sizeof(kMsgRows) / sizeof(kMsgRows[0]));
}

// #88: the row table pairs strings with ids by construction (shared X-macro
// list). The locale layer (M2-T003): msg() resolves through the current text
// locale, default EN; switching is explicit and reversible.
void test_msg_defaults_to_en(void) {
  setTextLocale(Locale::En);
  TEST_ASSERT_EQUAL_STRING(msgEn(Msg::BootBanner), msg(Msg::BootBanner));
}

void test_msg_de_locale_serves_de(void) {
  setTextLocale(Locale::De);
  TEST_ASSERT_EQUAL_STRING(msgDe(Msg::BootBanner), msg(Msg::BootBanner));
  setTextLocale(Locale::En);
  TEST_ASSERT_EQUAL_STRING(msgEn(Msg::BootBanner), msg(Msg::BootBanner));
}

void test_pairing_anchors(void) {
  TEST_ASSERT_NOT_NULL(strstr(msgEn(Msg::BootBanner), "test machine"));
  TEST_ASSERT_NOT_NULL(strstr(msgDe(Msg::BootBanner), "Pruefmaschinen"));
  TEST_ASSERT_NOT_NULL(strstr(msgEn(Msg::RpmSet), "Speed"));
  TEST_ASSERT_NOT_NULL(strstr(msgDe(Msg::RpmSet), "Drehzahl"));
  TEST_ASSERT_NOT_NULL(strstr(msgEn(Msg::FeedSet), "Feed"));
  TEST_ASSERT_NOT_NULL(strstr(msgDe(Msg::FeedSet), "Vorschub"));
  TEST_ASSERT_NOT_NULL(strstr(msgEn(Msg::OriginZeroed), "position"));
  TEST_ASSERT_NOT_NULL(strstr(msgDe(Msg::OriginZeroed), "Achsposition"));
}

void runTextTableTests(void) {
  RUN_TEST(test_msg_count_is_positive);
  RUN_TEST(test_every_msg_has_en_entry);
  RUN_TEST(test_every_msg_has_de_entry);
  RUN_TEST(test_table_covers_msg_exactly);
  RUN_TEST(test_pairing_anchors);
}
