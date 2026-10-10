// Unit tests for FirmwareCore command parsing (host side, env:native).
// Run: pio test -e native
//
// Semantics mirror the legacy String-based dispatch in src/main.cpp
// (behavior-preserving per M0 FR-004), including its quirks:
//   - command names are case-sensitive
// Non-numeric / truncated values: fixed per NQP #43 (InvalidValue).
// Word boundary after the value prefix: fixed per NQP #42.
#include <unity.h>

#include <FirmwareCore.h>

#define TEST_ASSERT_COMMAND(expected, actual) \
  TEST_ASSERT_EQUAL(static_cast<int>(expected), static_cast<int>(actual))

#define TEST_ASSERT_PARSE(expected, call) \
  TEST_ASSERT_EQUAL(static_cast<int>(expected), (call))

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
  // NQP #43 (fix): valid numbers keep working (acceptance criteria).
  float value = -1.0f;
  TEST_ASSERT_PARSE(ParseResult::Ok, parseCommandValue("rpm 5", "rpm", &value));
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 5.0f, value);

  TEST_ASSERT_PARSE(ParseResult::Ok,
                    parseCommandValue("turn 0.25", "turn", &value));
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.25f, value);

  TEST_ASSERT_PARSE(ParseResult::Ok,
                    parseCommandValue("move -2", "move", &value));
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, -2.0f, value);
}

void test_parse_command_value_trims_and_replaces_comma(void) {
  // German decimal comma is accepted (legacy replace(',', '.')).
  float value = -1.0f;
  TEST_ASSERT_PARSE(ParseResult::Ok,
                    parseCommandValue("feed 12,5", "feed", &value));
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 12.5f, value);

  TEST_ASSERT_PARSE(ParseResult::Ok, parseCommandValue("rpm   5", "rpm", &value));
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 5.0f, value);
}

void test_parse_command_value_prefix_mismatch(void) {
  // Not a command for this prefix: dispatch falls through to unknown.
  float value = -1.0f;
  TEST_ASSERT_PARSE(ParseResult::NotACommand,
                    parseCommandValue("rpm 5", "acc", &value));
}

void test_parse_command_value_empty_number_is_not_a_command(void) {
  // "rpm" or "rpm " has no number: legacy falls through to unknown
  // (acceptance: "rpm " stays unknown, unchanged by #43).
  float value = -1.0f;
  TEST_ASSERT_PARSE(ParseResult::NotACommand,
                    parseCommandValue("rpm", "rpm", &value));
  TEST_ASSERT_PARSE(ParseResult::NotACommand,
                    parseCommandValue("rpm ", "rpm", &value));
}

void test_parse_command_value_non_numeric_is_invalid(void) {
  // NQP #43 (fix, replaces the FR-004 pin "non_numeric_is_zero"):
  // legacy toFloat() yielded 0.0 for garbage, so the command "succeeded"
  // and the caller rejected it with the misleading "> 0" message.
  float value = -1.0f;
  TEST_ASSERT_PARSE(ParseResult::InvalidValue,
                    parseCommandValue("rpm abc", "rpm", &value));
  TEST_ASSERT_PARSE(ParseResult::InvalidValue,
                    parseCommandValue("acc x1", "acc", &value));
}

void test_parse_command_value_trailing_junk_is_invalid(void) {
  // NQP #43 (fix): the ENTIRE token must be consumed; legacy truncated
  // trailing junk ("rpm 5abc" silently became 5).
  float value = -1.0f;
  TEST_ASSERT_PARSE(ParseResult::InvalidValue,
                    parseCommandValue("rpm 5abc", "rpm", &value));
  TEST_ASSERT_PARSE(ParseResult::InvalidValue,
                    parseCommandValue("feed 1,2,3", "feed", &value));
  TEST_ASSERT_PARSE(ParseResult::InvalidValue,
                    parseCommandValue("move 2.5.5", "move", &value));
}

void test_parse_command_value_non_finite_is_invalid(void) {
  // strtof accepts "inf"/"nan" fully; they must not reach the step math
  // (lroundf(inf) is UB and an infinite setpoint is machine-unsafe).
  float value = -1.0f;
  TEST_ASSERT_PARSE(ParseResult::InvalidValue,
                    parseCommandValue("rpm inf", "rpm", &value));
  TEST_ASSERT_PARSE(ParseResult::InvalidValue,
                    parseCommandValue("move nan", "move", &value));
}

void test_parse_command_value_requires_word_boundary(void) {
  // NQP #42 (fix): the prefix must end at a word boundary (whitespace or
  // end-of-string), so longer words that merely start with the prefix are
  // unknown commands instead of parsing to 0.0.
  float value = -1.0f;
  TEST_ASSERT_PARSE(ParseResult::NotACommand,
                    parseCommandValue("rpmx 5", "rpm", &value));
  TEST_ASSERT_PARSE(ParseResult::NotACommand,
                    parseCommandValue("accident 5", "acc", &value));
  TEST_ASSERT_PARSE(ParseResult::NotACommand,
                    parseCommandValue("movable 2", "move", &value));

  // Valid commands keep working, including multiple spaces (acceptance).
  TEST_ASSERT_PARSE(ParseResult::Ok, parseCommandValue("rpm 5", "rpm", &value));
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 5.0f, value);
  TEST_ASSERT_PARSE(ParseResult::Ok, parseCommandValue("acc   2", "acc", &value));
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 2.0f, value);
}

void test_is_motion_command_named(void) {
  // FR-009: name commands that drive the motion system.
  TEST_ASSERT_TRUE(isMotionCommand("en"));
  TEST_ASSERT_TRUE(isMotionCommand("off"));
  TEST_ASSERT_TRUE(isMotionCommand("zero"));
  TEST_ASSERT_TRUE(isMotionCommand("stop"));
  TEST_ASSERT_TRUE(isMotionCommand("jog up"));
  TEST_ASSERT_TRUE(isMotionCommand("jog down"));
  TEST_ASSERT_TRUE(isMotionCommand("  en "));  // trimmed by caller, still true
}

void test_is_motion_command_prefix(void) {
  // Value commands with the NQP #42 word-boundary rule.
  TEST_ASSERT_TRUE(isMotionCommand("rpm 6"));
  TEST_ASSERT_TRUE(isMotionCommand("acc 20"));
  TEST_ASSERT_TRUE(isMotionCommand("feed 12"));
  TEST_ASSERT_TRUE(isMotionCommand("move 10"));
  TEST_ASSERT_TRUE(isMotionCommand("turn 2"));
  TEST_ASSERT_TRUE(isMotionCommand("turn"));  // bare prefix still drives motion
}

void test_is_motion_command_rejects_non_motion(void) {
  TEST_ASSERT_FALSE(isMotionCommand("pos"));
  TEST_ASSERT_FALSE(isMotionCommand("force"));
  TEST_ASSERT_FALSE(isMotionCommand("tare"));
  TEST_ASSERT_FALSE(isMotionCommand("cal_zero"));
  TEST_ASSERT_FALSE(isMotionCommand("cal_apply 100"));
  TEST_ASSERT_FALSE(isMotionCommand("bogus"));
  TEST_ASSERT_FALSE(isMotionCommand(""));
  TEST_ASSERT_FALSE(isMotionCommand(nullptr));
}

void test_is_motion_command_requires_word_boundary(void) {
  // Same boundary rule as the parser (NQP #42 pin): "rpmx 5" is not "rpm".
  TEST_ASSERT_FALSE(isMotionCommand("rpmx 5"));
  TEST_ASSERT_FALSE(isMotionCommand("feedx 1"));
  TEST_ASSERT_FALSE(isMotionCommand("turnkey"));
}

// --- parseLocaleAfterCommand (M2-T003, `lang <en|de>`) ----------------------
// Same dispatch grammar as parseCommandValue: word boundary required (#42),
// locale names exact and case-sensitive like command names (#41), the whole
// remainder after the prefix must be consumed.

void test_parse_locale_after_command_en_de(void) {
  Locale locale = Locale::En;
  TEST_ASSERT_EQUAL_INT(static_cast<int>(ParseResult::Ok),
                        static_cast<int>(parseLocaleAfterCommand("lang en", "lang", &locale)));
  TEST_ASSERT_EQUAL(static_cast<int>(Locale::En), static_cast<int>(locale));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(ParseResult::Ok),
                        static_cast<int>(parseLocaleAfterCommand("lang de", "lang", &locale)));
  TEST_ASSERT_EQUAL(static_cast<int>(Locale::De), static_cast<int>(locale));
}

void test_parse_locale_after_command_trims_whitespace(void) {
  Locale locale = Locale::En;
  TEST_ASSERT_EQUAL_INT(static_cast<int>(ParseResult::Ok),
                        static_cast<int>(parseLocaleAfterCommand("lang   de  ", "lang", &locale)));
  TEST_ASSERT_EQUAL(static_cast<int>(Locale::De), static_cast<int>(locale));
}

void test_parse_locale_after_command_bare_prefix_is_not_a_command(void) {
  // Mirrors `rpm` without a number: falls through to the unknown-command
  // path, which prints help - the operator sees the grammar.
  Locale locale = Locale::De;
  TEST_ASSERT_EQUAL_INT(static_cast<int>(ParseResult::NotACommand),
                        static_cast<int>(parseLocaleAfterCommand("lang", "lang", &locale)));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(ParseResult::NotACommand),
                        static_cast<int>(parseLocaleAfterCommand("lang   ", "lang", &locale)));
  TEST_ASSERT_EQUAL(static_cast<int>(Locale::De), static_cast<int>(locale));
}

void test_parse_locale_after_command_unknown_locale_is_invalid(void) {
  Locale locale = Locale::En;
  TEST_ASSERT_EQUAL_INT(static_cast<int>(ParseResult::InvalidValue),
                        static_cast<int>(parseLocaleAfterCommand("lang fr", "lang", &locale)));
  // Case-sensitive like every command name (NQP #41 pin).
  TEST_ASSERT_EQUAL_INT(static_cast<int>(ParseResult::InvalidValue),
                        static_cast<int>(parseLocaleAfterCommand("lang DE", "lang", &locale)));
  // Trailing junk: the entire remainder must be the locale token.
  TEST_ASSERT_EQUAL_INT(static_cast<int>(ParseResult::InvalidValue),
                        static_cast<int>(parseLocaleAfterCommand("lang de x", "lang", &locale)));
}

void test_parse_locale_after_command_prefix_mismatch(void) {
  Locale locale = Locale::En;
  // Word boundary (#42 pin): "langx" is not "lang".
  TEST_ASSERT_EQUAL_INT(static_cast<int>(ParseResult::NotACommand),
                        static_cast<int>(parseLocaleAfterCommand("langx de", "lang", &locale)));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(ParseResult::NotACommand),
                        static_cast<int>(parseLocaleAfterCommand("bogus en", "lang", &locale)));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(ParseResult::NotACommand),
                        static_cast<int>(parseLocaleAfterCommand("", "lang", &locale)));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(ParseResult::NotACommand),
                        static_cast<int>(parseLocaleAfterCommand(nullptr, "lang", &locale)));
}

void test_lang_is_not_a_motion_command(void) {
  // The locale must stay switchable in the Error state (M1 FR-009 rejects
  // motion commands only).
  TEST_ASSERT_FALSE(isMotionCommand("lang en"));
  TEST_ASSERT_FALSE(isMotionCommand("lang de"));
}

// #121 (M3-T007): the test-cycle commands. Exact-match semantics are pinned
// explicitly (home / "home top" / test resolve; glued or wrong-case spellings
// do NOT), because commandFromName runs before the value-prefix parsers -
// "test 5" must stay Unknown via exact match, not be swallowed by the
// testspeed/testarm/testdrop value parsers (#42 word-boundary quirk stays).
void test_m3_command_names_exact(void) {
  TEST_ASSERT_EQUAL_INT(static_cast<int>(Command::Home),
                        static_cast<int>(commandFromName("home")));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(Command::HomeTop),
                        static_cast<int>(commandFromName("home top")));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(Command::Test),
                        static_cast<int>(commandFromName("test")));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(Command::Home),
                        static_cast<int>(commandFromName("  home  ")));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(Command::Unknown),
                        static_cast<int>(commandFromName("hometop")));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(Command::Unknown),
                        static_cast<int>(commandFromName("Home")));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(Command::Unknown),
                        static_cast<int>(commandFromName("test 5")));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(Command::Unknown),
                        static_cast<int>(commandFromName("testdata")));
}

// M1 FR-009: the new commands drive the motion system and must be rejected
// in the Error state through isMotionCommand(). "testdata" (a readout) and
// "testdata 3" must NOT be swept up by the "test" entry.
void test_m3_motion_command_rejection(void) {
  TEST_ASSERT_TRUE(isMotionCommand("home"));
  TEST_ASSERT_TRUE(isMotionCommand("home top"));
  TEST_ASSERT_TRUE(isMotionCommand("test"));
  TEST_ASSERT_FALSE(isMotionCommand("testdata"));
  TEST_ASSERT_FALSE(isMotionCommand("testdata 3"));
  TEST_ASSERT_FALSE(isMotionCommand("hometop"));
}

// #121: `data <from>` resumes a sample dump at an index - unsigned integer
// only (no sign, no decimal point, no trailing junk), whole token consumed,
// same dispatch grammar as the value commands (#42 word boundary).
void test_parse_command_index_basic(void) {
  uint32_t index = 99;
  TEST_ASSERT_EQUAL_INT(static_cast<int>(ParseResult::Ok),
                        static_cast<int>(parseCommandIndex("data 5", "data", &index)));
  TEST_ASSERT_EQUAL_UINT32(5, index);
  index = 99;
  TEST_ASSERT_EQUAL_INT(static_cast<int>(ParseResult::Ok),
                        static_cast<int>(parseCommandIndex("data 0", "data", &index)));
  TEST_ASSERT_EQUAL_UINT32(0, index);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(ParseResult::Ok),
                        static_cast<int>(parseCommandIndex("data 42 ", "data", &index)));
  TEST_ASSERT_EQUAL_UINT32(42, index);
}

void test_parse_command_index_invalid(void) {
  uint32_t index = 7;
  TEST_ASSERT_EQUAL_INT(static_cast<int>(ParseResult::InvalidValue),
                        static_cast<int>(parseCommandIndex("data -1", "data", &index)));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(ParseResult::InvalidValue),
                        static_cast<int>(parseCommandIndex("data abc", "data", &index)));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(ParseResult::InvalidValue),
                        static_cast<int>(parseCommandIndex("data 1.5", "data", &index)));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(ParseResult::InvalidValue),
                        static_cast<int>(parseCommandIndex("data 3junk", "data", &index)));
  // bare prefix: unknown-command path (help), not an invalid value
  TEST_ASSERT_EQUAL_INT(static_cast<int>(ParseResult::NotACommand),
                        static_cast<int>(parseCommandIndex("data", "data", &index)));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(ParseResult::NotACommand),
                        static_cast<int>(parseCommandIndex("datax 3", "data", &index)));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(ParseResult::NotACommand),
                        static_cast<int>(parseCommandIndex("pos", "data", &index)));
}

void runCommandParserTests() {
  RUN_TEST(test_command_from_name_exact);
  RUN_TEST(test_command_from_name_trims_whitespace);
  RUN_TEST(test_command_from_name_is_case_sensitive);
  RUN_TEST(test_command_from_name_is_exact_match_only);
  RUN_TEST(test_parse_command_value_basic);
  RUN_TEST(test_parse_command_value_trims_and_replaces_comma);
  RUN_TEST(test_parse_command_value_prefix_mismatch);
  RUN_TEST(test_parse_command_value_empty_number_is_not_a_command);
  RUN_TEST(test_parse_command_value_non_numeric_is_invalid);
  RUN_TEST(test_parse_command_value_trailing_junk_is_invalid);
  RUN_TEST(test_parse_command_value_non_finite_is_invalid);
  RUN_TEST(test_parse_command_value_requires_word_boundary);
  RUN_TEST(test_is_motion_command_named);
  RUN_TEST(test_is_motion_command_prefix);
  RUN_TEST(test_is_motion_command_rejects_non_motion);
  RUN_TEST(test_is_motion_command_requires_word_boundary);
  RUN_TEST(test_parse_locale_after_command_en_de);
  RUN_TEST(test_parse_locale_after_command_trims_whitespace);
  RUN_TEST(test_parse_locale_after_command_bare_prefix_is_not_a_command);
  RUN_TEST(test_parse_locale_after_command_unknown_locale_is_invalid);
  RUN_TEST(test_parse_locale_after_command_prefix_mismatch);
  RUN_TEST(test_lang_is_not_a_motion_command);
  RUN_TEST(test_m3_command_names_exact);
  RUN_TEST(test_m3_motion_command_rejection);
  RUN_TEST(test_parse_command_index_basic);
  RUN_TEST(test_parse_command_index_invalid);
}
