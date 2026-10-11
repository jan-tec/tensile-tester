// Unit tests for the pure locale resolution (host side, env:native).
// Run: pio test -e native
//
// M2 TC-001 (T001, RED-first): the locale model and string-table
// resolution live in FirmwareCore so they are host-testable:
// - localeFromName(): "en"/"de" parse; NULL/empty/unknown -> En (the
//   FR-002 default, also the NVS-missing-key case).
// - resolveMsgTable(): which string table serves a message:
//   0 = EN table, 1 = DE table. A missing DE entry falls back to the
//   EN table even in De locale; En never resolves to DE.
#include <unity.h>

#include <FirmwareCore.h>

void test_locale_from_name_en(void) {
  TEST_ASSERT_EQUAL(static_cast<int>(Locale::En),
                    static_cast<int>(localeFromName("en")));
}

void test_locale_from_name_de(void) {
  TEST_ASSERT_EQUAL(static_cast<int>(Locale::De),
                    static_cast<int>(localeFromName("de")));
}

void test_locale_from_name_null_is_default_en(void) {
  TEST_ASSERT_EQUAL(static_cast<int>(Locale::En),
                    static_cast<int>(localeFromName(nullptr)));
}

void test_locale_from_name_empty_is_default_en(void) {
  TEST_ASSERT_EQUAL(static_cast<int>(Locale::En),
                    static_cast<int>(localeFromName("")));
}

void test_locale_from_name_unknown_is_default_en(void) {
  TEST_ASSERT_EQUAL(static_cast<int>(Locale::En),
                    static_cast<int>(localeFromName("fr")));
}

void test_resolve_de_with_entry_serves_de_table(void) {
  TEST_ASSERT_EQUAL(1, resolveMsgTable(Locale::De, true));
}

void test_resolve_de_missing_entry_falls_back_to_en(void) {
  TEST_ASSERT_EQUAL(0, resolveMsgTable(Locale::De, false));
}

void test_resolve_en_stays_en_even_without_de_entry(void) {
  TEST_ASSERT_EQUAL(0, resolveMsgTable(Locale::En, false));
}

void test_resolve_en_with_de_entry_still_en(void) {
  TEST_ASSERT_EQUAL(0, resolveMsgTable(Locale::En, true));
}

// Settings persists the locale under this exact NVS name (M2-T003);
// localeFromName(localeName(x)) must round-trip.
void test_locale_name_round_trip(void) {
  TEST_ASSERT_EQUAL_STRING("en", localeName(Locale::En));
  TEST_ASSERT_EQUAL_STRING("de", localeName(Locale::De));
  TEST_ASSERT_EQUAL(static_cast<int>(Locale::En),
                    static_cast<int>(localeFromName(localeName(Locale::En))));
  TEST_ASSERT_EQUAL(static_cast<int>(Locale::De),
                    static_cast<int>(localeFromName(localeName(Locale::De))));
}

void runLocaleTests() {
  RUN_TEST(test_locale_from_name_en);
  RUN_TEST(test_locale_from_name_de);
  RUN_TEST(test_locale_from_name_null_is_default_en);
  RUN_TEST(test_locale_from_name_empty_is_default_en);
  RUN_TEST(test_locale_from_name_unknown_is_default_en);
  RUN_TEST(test_resolve_de_with_entry_serves_de_table);
  RUN_TEST(test_resolve_de_missing_entry_falls_back_to_en);
  RUN_TEST(test_resolve_en_stays_en_even_without_de_entry);
  RUN_TEST(test_resolve_en_with_de_entry_still_en);
  RUN_TEST(test_locale_name_round_trip);
}
