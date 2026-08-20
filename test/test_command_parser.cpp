// Unit tests for FirmwareCore command parsing (host side, env:native).
// Run: pio test -e native
//
// Semantics mirror the legacy String-based dispatch in src/main.cpp
// (behavior-preserving per M0 FR-004), including its quirks:
//   - command names are case-sensitive
//   - value commands match by prefix WITHOUT a word boundary
//   - a non-numeric value parses to 0.0 (caller rejects <= 0)
#include <unity.h>

#include <FirmwareCore.h>

#define TEST_ASSERT_COMMAND(expected, actual) \
  TEST_ASSERT_EQUAL(static_cast<int>(expected), static_cast<int>(actual))

void test_command_from_name_exact(void) {
  TEST_ASSERT_COMMAND(Command::Enable, commandFromName("en"));
  TEST_ASSERT_COMMAND(Command::Disable, commandFromName("off"));
  TEST_ASSERT_COMMAND(Command::Zero, commandFromName("zero"));
  TEST_ASSERT_COMMAND(Command::Tare, commandFromName("tare"));
  TEST_ASSERT_COMMAND(Command::Force, commandFromName("force"));
  TEST_ASSERT_COMMAND(Command::Stop, commandFromName("stop"));
  TEST_ASSERT_COMMAND(Command::Pos, commandFromName("pos"));
  TEST_ASSERT_COMMAND(Command::CalZero, commandFromName("cal_zero"));
  TEST_ASSERT_COMMAND(Command::JogUp, commandFromName("jog up"));
  TEST_ASSERT_COMMAND(Command::JogDown, commandFromName("jog down"));
}

void test_command_from_name_trims_whitespace(void) {
  TEST_ASSERT_COMMAND(Command::Enable, commandFromName("  en  "));
  TEST_ASSERT_COMMAND(Command::JogUp, commandFromName("\tjog up\r\n"));
  TEST_ASSERT_COMMAND(Command::Unknown, commandFromName("   "));
}

void test_command_from_name_is_case_sensitive(void) {
  // Legacy dispatch compares exact case; "EN" is an unknown command.
  TEST_ASSERT_COMMAND(Command::Unknown, commandFromName("EN"));
  TEST_ASSERT_COMMAND(Command::Unknown, commandFromName("JOG UP"));
}

void test_command_from_name_is_exact_match_only(void) {
  // Value commands are not names; a name never matches with extra tokens.
  TEST_ASSERT_COMMAND(Command::Unknown, commandFromName("rpm 5"));
  TEST_ASSERT_COMMAND(Command::Unknown, commandFromName("en 1"));
  TEST_ASSERT_COMMAND(Command::Unknown, commandFromName(""));
  TEST_ASSERT_COMMAND(Command::Unknown, commandFromName("jog"));
}

void test_parse_command_value_basic(void) {
  float value = -1.0f;
  TEST_ASSERT_TRUE(parseCommandValue("rpm 5", "rpm", &value));
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 5.0f, value);

  TEST_ASSERT_TRUE(parseCommandValue("turn 0.25", "turn", &value));
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.25f, value);

  TEST_ASSERT_TRUE(parseCommandValue("move -2", "move", &value));
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, -2.0f, value);
}

void test_parse_command_value_trims_and_replaces_comma(void) {
  // German decimal comma is accepted (legacy replace(',', '.')).
  float value = -1.0f;
  TEST_ASSERT_TRUE(parseCommandValue("feed 12,5", "feed", &value));
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 12.5f, value);

  TEST_ASSERT_TRUE(parseCommandValue("rpm   5", "rpm", &value));
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 5.0f, value);
}

void test_parse_command_value_prefix_mismatch(void) {
  float value = -1.0f;
  TEST_ASSERT_FALSE(parseCommandValue("rpm 5", "acc", &value));
}

void test_parse_command_value_empty_number_is_false(void) {
  // "rpm" or "rpm " has no number: legacy falls through to unknown.
  float value = -1.0f;
  TEST_ASSERT_FALSE(parseCommandValue("rpm", "rpm", &value));
  TEST_ASSERT_FALSE(parseCommandValue("rpm ", "rpm", &value));
}

void test_parse_command_value_non_numeric_is_zero(void) {
  // Quirk pinned (FR-004): legacy toFloat() yields 0.0 for garbage, so the
  // command "succeeds" and the caller rejects it with the > 0 check.
  float value = -1.0f;
  TEST_ASSERT_TRUE(parseCommandValue("rpm abc", "rpm", &value));
  TEST_ASSERT_FLOAT_WITHIN(0.0f, 0.0f, value);
}

void test_parse_command_value_prefix_has_no_word_boundary(void) {
  // Quirk pinned (FR-004): legacy startsWith("rpm") matches "rpmx 5" too.
  // Parsing the remainder ("x 5") yields 0.0; caller rejects via > 0.
  float value = -1.0f;
  TEST_ASSERT_TRUE(parseCommandValue("rpmx 5", "rpm", &value));
  TEST_ASSERT_FLOAT_WITHIN(0.0f, 0.0f, value);
}

void runCommandParserTests() {
  RUN_TEST(test_command_from_name_exact);
  RUN_TEST(test_command_from_name_trims_whitespace);
  RUN_TEST(test_command_from_name_is_case_sensitive);
  RUN_TEST(test_command_from_name_is_exact_match_only);
  RUN_TEST(test_parse_command_value_basic);
  RUN_TEST(test_parse_command_value_trims_and_replaces_comma);
  RUN_TEST(test_parse_command_value_prefix_mismatch);
  RUN_TEST(test_parse_command_value_empty_number_is_false);
  RUN_TEST(test_parse_command_value_non_numeric_is_zero);
  RUN_TEST(test_parse_command_value_prefix_has_no_word_boundary);
}
