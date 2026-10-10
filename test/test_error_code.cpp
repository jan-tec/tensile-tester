// Unit tests for the selftest vocabulary helpers (host side, env:native).
// Run: pio test -e native
//
// M1 FR-008 (T010, RED-first): the overall rating of the /api/status
// selftest object - any FAIL dominates, otherwise any WARN, and
// NOT_WIRED / NOT_CONNECTED are info only (design D3: absence is not a
// fault, never FAIL, never lowers the overall rating).
#include <unity.h>

#include <cstring>

#include <ErrorCode.h>

void test_overall_all_ok(void) {
  const CheckStatus statuses[3] = {
      CheckStatus::Ok, CheckStatus::Ok, CheckStatus::Ok};
  TEST_ASSERT_EQUAL(static_cast<int>(CheckStatus::Ok),
                    static_cast<int>(overallStatus(statuses, 3)));
}

void test_overall_fail_dominates(void) {
  const CheckStatus statuses[4] = {
      CheckStatus::Warn, CheckStatus::Ok,
      CheckStatus::Fail, CheckStatus::Ok};
  TEST_ASSERT_EQUAL(static_cast<int>(CheckStatus::Fail),
                    static_cast<int>(overallStatus(statuses, 4)));
}

void test_overall_warn_when_no_fail(void) {
  const CheckStatus statuses[2] = {CheckStatus::Ok, CheckStatus::Warn};
  TEST_ASSERT_EQUAL(static_cast<int>(CheckStatus::Warn),
                    static_cast<int>(overallStatus(statuses, 2)));
}

void test_overall_info_statuses_do_not_lower(void) {
  const CheckStatus statuses[3] = {
      CheckStatus::Ok, CheckStatus::NotWired, CheckStatus::NotConnected};
  TEST_ASSERT_EQUAL(static_cast<int>(CheckStatus::Ok),
                    static_cast<int>(overallStatus(statuses, 3)));
}

void test_overall_empty_is_ok(void) {
  const CheckStatus statuses[1] = {CheckStatus::Ok};
  TEST_ASSERT_EQUAL(static_cast<int>(CheckStatus::Ok),
                    static_cast<int>(overallStatus(statuses, 0)));
}

// FR-007 machine-readable grammar: codes are 4 padded upper-case hex digits.
// `Serial.print(v, HEX)` drops the leading zeros, so 0x0301 printed as
// `0x301` and broke fixed-width log parsers (issue #97).
void test_error_code_hex_pads_to_four_digits(void) {
  char buf[7];
  TEST_ASSERT_EQUAL_STRING("0x0000", errorCodeHex(0x0000, buf));
  TEST_ASSERT_EQUAL_STRING("0x0301", errorCodeHex(0x301, buf));
  TEST_ASSERT_EQUAL_STRING("0x0F0A", errorCodeHex(0xF0A, buf));
  TEST_ASSERT_EQUAL_STRING("0xFFFF", errorCodeHex(0xFFFF, buf));
}

void test_error_code_hex_fixed_width(void) {
  // Fixed width is the whole point: `prefix + " code=" + errorCodeHex(v)`
  // must not shift the columns that follow.
  char buf[7];
  for (unsigned i = 0; i < 4; ++i) {
    errorCodeHex(static_cast<uint16_t>(1u << (i * 4)), buf);
    TEST_ASSERT_EQUAL_UINT(6u, static_cast<unsigned>(strlen(buf)));
    TEST_ASSERT_EQUAL_CHAR('0', buf[0]);
    TEST_ASSERT_EQUAL_CHAR('x', buf[1]);
  }
}

// #90: M3 raises the selftest set to 7 checks (endstops). The pure
// overallStatus() is count-driven (the WebUi JSON path was the bug: a fixed
// 5-slot gather); pin the >5 behaviour here so a future re-cap cannot hide a
// FAILing 6th/7th row from the overall rating (M1 FR-003 safety signal).
void test_overall_seventh_check_fail_wins(void) {
  const CheckStatus statuses[7] = {
      CheckStatus::Ok, CheckStatus::Ok, CheckStatus::Warn, CheckStatus::Ok,
      CheckStatus::Ok, CheckStatus::Ok, CheckStatus::Fail};
  TEST_ASSERT_EQUAL_INT(static_cast<int>(CheckStatus::Fail),
                        static_cast<int>(overallStatus(statuses, 7)));
}

void test_overall_seventh_check_warn_lowers_to_warn(void) {
  const CheckStatus statuses[7] = {
      CheckStatus::Ok, CheckStatus::Ok, CheckStatus::Ok, CheckStatus::Ok,
      CheckStatus::Ok, CheckStatus::Ok, CheckStatus::Warn};
  TEST_ASSERT_EQUAL_INT(static_cast<int>(CheckStatus::Warn),
                        static_cast<int>(overallStatus(statuses, 7)));
}

// #122 (M3-T006): the 0x05xx endstop block reserved by the M3 homing/test
// cycle. Values are frozen the moment they ship - never renumber; names
// follow the existing UPPER_SNAKE grammar (FR-007 log surface).
void test_endstop_error_block(void) {
  TEST_ASSERT_EQUAL_UINT16(
      0x0501, static_cast<uint16_t>(ErrorCode::EndstopSeekTimeout));
  TEST_ASSERT_EQUAL_UINT16(
      0x0502, static_cast<uint16_t>(ErrorCode::EndstopInstantTrigger));
  TEST_ASSERT_EQUAL_UINT16(
      0x0503, static_cast<uint16_t>(ErrorCode::EndstopStuckSwitch));
  TEST_ASSERT_EQUAL_UINT16(
      0x0504, static_cast<uint16_t>(ErrorCode::EndstopPressedOrBroken));
  TEST_ASSERT_EQUAL_STRING("ENDSTOP_SEEK_TIMEOUT",
                           errorCodeName(ErrorCode::EndstopSeekTimeout));
  TEST_ASSERT_EQUAL_STRING("ENDSTOP_INSTANT_TRIGGER",
                           errorCodeName(ErrorCode::EndstopInstantTrigger));
  TEST_ASSERT_EQUAL_STRING("ENDSTOP_STUCK_SWITCH",
                           errorCodeName(ErrorCode::EndstopStuckSwitch));
  TEST_ASSERT_EQUAL_STRING(
      "ENDSTOP_PRESSED_OR_BROKEN",
      errorCodeName(ErrorCode::EndstopPressedOrBroken));
}

void runErrorCodeTests() {
  RUN_TEST(test_overall_all_ok);
  RUN_TEST(test_overall_fail_dominates);
  RUN_TEST(test_overall_warn_when_no_fail);
  RUN_TEST(test_overall_info_statuses_do_not_lower);
  RUN_TEST(test_overall_empty_is_ok);
  RUN_TEST(test_overall_seventh_check_fail_wins);
  RUN_TEST(test_overall_seventh_check_warn_lowers_to_warn);
  RUN_TEST(test_endstop_error_block);
  RUN_TEST(test_error_code_hex_pads_to_four_digits);
  RUN_TEST(test_error_code_hex_fixed_width);
}
