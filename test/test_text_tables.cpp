// Unit tests for the M2 message-string tables (host side, env:native).
// Run: pio test -e native
//
// M2 TC-002 (T002, RED-first): every Msg id must carry a non-empty EN and a
// non-empty DE entry, so a half-translated table cannot ship. The tables live
// in src/Text.h as header-inline pointer arrays: the host test links them
// directly (src/*.cpp is not part of the native test build, same pattern as
// the pure ErrorCode.h). PROGMEM is a no-op on both the host and ESP32
// (unified address space), so plain indexing is valid everywhere.
#include <unity.h>

#include <Text.h>

#include <cstddef>
#include <cstdint>

void test_msg_count_is_positive(void) {
  TEST_ASSERT_GREATER_THAN_INT(0, static_cast<int>(Msg::Count));
}

void test_every_msg_has_en_entry(void) {
  for (int i = 0; i < static_cast<int>(Msg::Count); ++i) {
    const char *entry = kMsgEn[i];
    TEST_ASSERT_NOT_NULL_MESSAGE(entry, "Msg id has no EN entry");
    TEST_ASSERT_TRUE_MESSAGE(entry[0] != '\0', "Msg id has an empty EN entry");
  }
}

void test_every_msg_has_de_entry(void) {
  for (int i = 0; i < static_cast<int>(Msg::Count); ++i) {
    const char *entry = kMsgDe[i];
    TEST_ASSERT_NOT_NULL_MESSAGE(entry, "Msg id has no DE entry");
    TEST_ASSERT_TRUE_MESSAGE(entry[0] != '\0', "Msg id has an empty DE entry");
  }
}

// EN and DE tables must be parallel: same length, index-wise pairing.
void test_tables_are_the_same_length(void) {
  const uint32_t en = sizeof(kMsgEn) / sizeof(kMsgEn[0]);
  const uint32_t de = sizeof(kMsgDe) / sizeof(kMsgDe[0]);
  TEST_ASSERT_EQUAL_UINT32(en, de);
  TEST_ASSERT_EQUAL_UINT32(static_cast<uint32_t>(Msg::Count), en);
}

void runTextTableTests(void) {
  RUN_TEST(test_msg_count_is_positive);
  RUN_TEST(test_every_msg_has_en_entry);
  RUN_TEST(test_every_msg_has_de_entry);
  RUN_TEST(test_tables_are_the_same_length);
}
