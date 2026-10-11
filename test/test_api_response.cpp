// Unit tests for the M6 serial API envelope (host side, env:native).
// Run: pio test -e native
//
// #178 (M6-T002), spec FR-001/FR-002, TC-001/TC-002: the response line
// is a wire contract - fixed key order, locale-neutral decimals, ASCII
// escaping, <=256 B. Pinned as exact strings so any formatter drift is
// a host test failure, not a bench surprise.
#include <unity.h>

#include <cstring>

#include <ApiResponse.h>

static ApiStateSnapshot benchSnapshot() {
  // Post-B1 reality 2026-10-11: homed, calibrated, broken sample gone,
  // Idle, no fault (numbers from the #48 calibration session).
  ApiStateSnapshot s{};
  s.posMm = 11.666f;
  s.forceN = 155.71f;
  s.cellReady = true;
  s.calValid = true;
  s.homed = true;
  s.faultCode = 0;
  s.cycleState = "Idle";
  return s;
}

// TC-001: exact success envelope, fixed key order ok -> data -> state.
void test_success_envelope_exact(void) {
  char out[256];
  const int n = formatApiSuccess(benchSnapshot(), "{}", out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING(
      "{\"ok\":true,\"data\":{},\"state\":{\"posMm\":11.666,\"forceN\":155.71,"
      "\"calValid\":true,\"cycleState\":\"Idle\",\"faultCode\":0,"
      "\"homed\":true}}",
      out);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(strlen(out)), n);
}

// TC-001: error envelope - decimal code, escaped message, state last.
void test_error_envelope_exact(void) {
  char out[256];
  const int n =
      formatApiError(benchSnapshot(), 0x0601, "unknown command", out,
                     sizeof(out));
  TEST_ASSERT_EQUAL_STRING(
      "{\"ok\":false,\"error\":{\"code\":1537,"
      "\"message\":\"unknown command\"},\"state\":{\"posMm\":11.666,"
      "\"forceN\":155.71,\"calValid\":true,\"cycleState\":\"Idle\","
      "\"faultCode\":0,\"homed\":true}}",
      out);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(strlen(out)), n);
}

// TC-002: unhomed + cell not ready -> null members (the n/a prose of the
// old multi-line status becomes JSON null, FR-002).
void test_state_null_members_when_unknown(void) {
  char out[256];
  ApiStateSnapshot s = benchSnapshot();
  s.homed = false;
  s.cellReady = false;
  s.calValid = false;
  s.cycleState = "Idle";
  formatApiSuccess(s, "null", out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING(
      "{\"ok\":true,\"data\":null,\"state\":{\"posMm\":null,\"forceN\":null,"
      "\"calValid\":false,\"cycleState\":\"Idle\",\"faultCode\":0,"
      "\"homed\":false}}",
      out);
}

// TC-002: fault + cycle states render without special casing.
void test_state_fault_and_cycle(void) {
  char out[256];
  ApiStateSnapshot s = benchSnapshot();
  s.faultCode = 0x0501;
  s.cycleState = "Pulling";
  formatApiStateJson(s, out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING(
      "{\"posMm\":11.666,\"forceN\":155.71,\"calValid\":true,"
      "\"cycleState\":\"Pulling\",\"faultCode\":1281,\"homed\":true}",
      out);
}

// TC-001: escaping - quote, backslash, newline, control char.
void test_error_message_escaping(void) {
  char out[256];
  formatApiError(benchSnapshot(), 0x0602, "bad \"x\\y\"\n\t\x01", out,
                 sizeof(out));
  TEST_ASSERT_NOT_NULL(strstr(out, "\\\"x\\\\y\\\"\\n\\t\\u0001"));
}

// D3: async lines carry a top-level event key, state second.
void test_event_line_exact(void) {
  char out[256];
  formatApiEvent("status", benchSnapshot(), out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING(
      "{\"event\":\"status\",\"state\":{\"posMm\":11.666,\"forceN\":155.71,"
      "\"calValid\":true,\"cycleState\":\"Idle\",\"faultCode\":0,"
      "\"homed\":true}}",
      out);
}

// NFR: realistic lines stay under the 256 B bound.
void test_length_bound_realistic_lines(void) {
  char out[256];
  const int errLen = formatApiError(
      benchSnapshot(), 0x0603,
      "cycle active: command rejected because a test cycle is running",
      out, sizeof(out));
  TEST_ASSERT_TRUE(errLen > 0 && errLen < 256);
  const int okLen = formatApiSuccess(benchSnapshot(), "{\"reply\":\"ok\"}",
                                     out, sizeof(out));
  TEST_ASSERT_TRUE(okLen > 0 && okLen < 256);
}

// snprintf truncation convention: a too-small buffer signals the
// would-be length (>= cap), never a silent partial line.
void test_truncation_signalled(void) {
  char out[16];
  const int n = formatApiSuccess(benchSnapshot(), "{}", out, sizeof(out));
  TEST_ASSERT_TRUE(n >= static_cast<int>(sizeof(out)));
}

// Negative numbers format through the same locale-neutral path (a
// hanging-mass tare reads negative force, seen on the bench 2026-10-11).
void test_negative_force_renders(void) {
  char out[256];
  ApiStateSnapshot s = benchSnapshot();
  s.forceN = -24.1f;
  formatApiStateJson(s, out, sizeof(out));
  TEST_ASSERT_NOT_NULL(strstr(out, "\"forceN\":-24.10"));
}

void runApiResponseTests() {
  RUN_TEST(test_success_envelope_exact);
  RUN_TEST(test_error_envelope_exact);
  RUN_TEST(test_state_null_members_when_unknown);
  RUN_TEST(test_state_fault_and_cycle);
  RUN_TEST(test_error_message_escaping);
  RUN_TEST(test_event_line_exact);
  RUN_TEST(test_length_bound_realistic_lines);
  RUN_TEST(test_truncation_signalled);
  RUN_TEST(test_negative_force_renders);
}
