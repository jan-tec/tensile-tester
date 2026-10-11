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

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iterator>

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

// M2-T004 (FR-007): every stable ErrorCode resolves to a presentable Msg,
// and that message localizes in both locales (TC-003 "POWER_LOW-style
// message localized in EN and DE").
void test_msg_for_fault_maps_every_error_code(void) {
  TEST_ASSERT_EQUAL_INT(static_cast<int>(Msg::FaultNvsOpen),
                        static_cast<int>(msgForFault(ErrorCode::NvsOpenFailed)));
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(Msg::FaultNvsCorrupt),
      static_cast<int>(msgForFault(ErrorCode::NvsCalibrationCorrupt)));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(Msg::FaultPowerLow),
                        static_cast<int>(msgForFault(ErrorCode::PowerLow)));
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(Msg::FaultHx711NoVariation),
      static_cast<int>(msgForFault(ErrorCode::Hx711NoVariation)));
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(Msg::FaultMotorNoFeedback),
      static_cast<int>(msgForFault(ErrorCode::MotorNoFeedback)));
  // M3 endstop block (0x0501-0x0504, bench-found 2026-10-11: an endstop
  // fault showed "Unknown fault" in the Error-state reply).
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(Msg::FaultEndstopSeekTimeout),
      static_cast<int>(msgForFault(ErrorCode::EndstopSeekTimeout)));
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(Msg::FaultEndstopInstantTrigger),
      static_cast<int>(msgForFault(ErrorCode::EndstopInstantTrigger)));
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(Msg::FaultEndstopStuckSwitch),
      static_cast<int>(msgForFault(ErrorCode::EndstopStuckSwitch)));
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(Msg::FaultEndstopPressedOrBroken),
      static_cast<int>(msgForFault(ErrorCode::EndstopPressedOrBroken)));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(Msg::FaultUnknown),
                        static_cast<int>(msgForFault(static_cast<ErrorCode>(0xBEEF))));
}

void test_fault_details_localize_in_both_locales(void) {
  setTextLocale(Locale::En);
  TEST_ASSERT_NOT_NULL(strstr(msg(msgForFault(ErrorCode::PowerLow)), "Supply voltage"));
  TEST_ASSERT_NOT_NULL(strstr(msg(msgForFault(ErrorCode::NvsOpenFailed)), "NVS"));
  setTextLocale(Locale::De);
  TEST_ASSERT_NOT_NULL(strstr(msg(msgForFault(ErrorCode::PowerLow)), "Versorgung"));
  TEST_ASSERT_NOT_NULL(strstr(msg(msgForFault(ErrorCode::NvsOpenFailed)), "NVS"));
  setTextLocale(Locale::En);
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

// #89: a DE entry copied verbatim from EN would satisfy the non-empty walk
// above while serving English to German users. Identical EN/DE text is only
// legitimate for the locale-invariant ids listed here (units, the "n/a"
// token, the command-name list - strings no locale ever retranslates); a
// prose entry copied to DE must fail this test.
void test_de_is_translated_except_invariants(void) {
  static const Msg kInvariant[] = {
      Msg::MillimetreSuffix, Msg::MillimetrePerMinuteSuffix, Msg::NewtonSuffix,
      Msg::PercentSuffix, Msg::SecondsSuffix,
      Msg::HelpCommandsSuffix,
  };
  for (int i = 0; i < static_cast<int>(Msg::Count); ++i) {
    const Msg id = static_cast<Msg>(i);
    if (strcmp(msgEn(id), msgDe(id)) != 0) {
      continue;  // translated, fine
    }
    const bool allowed =
        std::find(std::begin(kInvariant), std::end(kInvariant), id) !=
        std::end(kInvariant);
    TEST_ASSERT_TRUE_MESSAGE(allowed,
                             "DE entry is a copy of EN (untranslated?)");
  }
}

void runTextTableTests(void) {
  RUN_TEST(test_msg_count_is_positive);
  RUN_TEST(test_every_msg_has_en_entry);
  RUN_TEST(test_every_msg_has_de_entry);
  RUN_TEST(test_table_covers_msg_exactly);
  RUN_TEST(test_msg_defaults_to_en);
  RUN_TEST(test_msg_de_locale_serves_de);
  RUN_TEST(test_msg_for_fault_maps_every_error_code);
  RUN_TEST(test_fault_details_localize_in_both_locales);
  RUN_TEST(test_de_is_translated_except_invariants);
  RUN_TEST(test_pairing_anchors);
}
