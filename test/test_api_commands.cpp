// Unit tests for the M6 API command surface (host side, env:native).
// Run: pio test -e native
//
// #180 (M6-T004), spec FR-003, TC-003: the command table drives `help`
// (JSON array in data, D6) and is swept so every current-surface
// command renders a parseable envelope in both success and error
// shapes. Phasing pin (plan D5): M4/M5 commands are NOT in the surface
// yet - they arrive envelope-native in their milestones.
#include <unity.h>

#include <cstring>

#include <ApiResponse.h>

static ApiStateSnapshot snap() {
  ApiStateSnapshot s{};
  s.posMm = 1.0f;
  s.forceN = 2.0f;
  s.cellReady = true;
  s.calValid = true;
  s.homed = true;
  s.faultCode = 0;
  s.cycleState = "Idle";
  return s;
}

// Minimal JSON validator: balanced braces/brackets outside strings,
// balanced quotes, non-empty. Host proxy for "json.loads succeeds".
static bool jsonBalanced(const char* s) {
  long braces = 0, brackets = 0;
  bool inString = false;
  if (s == nullptr || *s == '\0') {
    return false;
  }
  for (const char* p = s; *p != '\0'; ++p) {
    if (inString) {
      if (*p == '\\') {
        ++p;
        if (*p == '\0') {
          return false;
        }
      } else if (*p == '"') {
        inString = false;
      }
      continue;
    }
    switch (*p) {
      case '"':
        inString = true;
        break;
      case '{':
        ++braces;
        break;
      case '}':
        --braces;
        break;
      case '[':
        ++brackets;
        break;
      case ']':
        --brackets;
        break;
      default:
        break;
    }
  }
  return !inString && braces == 0 && brackets == 0;
}

// FR-003 current surface, pinned exactly (order = help order).
void test_help_array_exact(void) {
  char out[512];
  formatApiHelpData(out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING(
      "[\"en\",\"off\",\"zero\",\"tare\",\"pos\",\"force\",\"stop\","
      "\"rpm <U/min>\",\"acc <U/min/s>\",\"feed <mm/min>\",\"move <mm>\","
      "\"turn <rev>\",\"jog up\",\"jog down\",\"help\",\"cal_zero\","
      "\"cal_apply <grams>\",\"home\",\"home top\",\"test\","
      "\"testspeed <mm/min>\",\"testarm <N>\",\"testdrop <%>\","
      "\"data [from]\",\"testdata\",\"homeseek <mm/min>\","
      "\"hometimeout <s>\",\"ends\",\"lang <en|de>\"]",
      out);
}

// D6: the full help line is the documented >256 B exception - it must
// still render completely (no truncation) in a caller-provided buffer.
void test_help_full_line_renders(void) {
  char help[512];
  formatApiHelpData(help, sizeof(help));
  char out[768];
  const int n = formatApiSuccess(snap(), help, out, sizeof(out));
  TEST_ASSERT_TRUE(n > 256);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(strlen(out)), n);
  TEST_ASSERT_TRUE(jsonBalanced(out));
  TEST_ASSERT_EQUAL_CHAR('}', out[strlen(out) - 1]);
}

// Phasing (plan D5): M4/M5 commands must not appear in the surface yet.
void test_m4_m5_commands_absent(void) {
  char help[512];
  formatApiHelpData(help, sizeof(help));
  TEST_ASSERT_NULL(strstr(help, "calibrate"));
  TEST_ASSERT_NULL(strstr(help, "cal_next"));
  TEST_ASSERT_NULL(strstr(help, "chold"));
  TEST_ASSERT_NULL(strstr(help, "choltol"));
  TEST_ASSERT_NULL(strstr(help, "choldclear"));
}

// No command appears twice in help (a duplicated row would confuse both
// operators and hosts).
void test_no_duplicate_commands(void) {
  const size_t n = apiCommandCount();
  TEST_ASSERT_TRUE(n > 0);
  for (size_t i = 0; i < n; ++i) {
    for (size_t j = i + 1; j < n; ++j) {
      // Unity has no NOT_EQUAL_STRING: strcmp != 0 is the same pin.
      TEST_ASSERT_TRUE_MESSAGE(
          strcmp(apiCommandAt(i).name, apiCommandAt(j).name) != 0,
          apiCommandAt(i).name);
    }
  }
}

// TC-003 sweep: every table entry renders balanced envelopes in both
// shapes, with the entry name as a data payload (success) and the
// InvalidValue code path (error).
void test_surface_envelope_sweep(void) {
  const size_t n = apiCommandCount();
  for (size_t i = 0; i < n; ++i) {
    char data[96];
    snprintf(data, sizeof(data), "{\"reply\":\"%s\"}", apiCommandAt(i).name);
    char ok[256];
    formatApiSuccess(snap(), data, ok, sizeof(ok));
    TEST_ASSERT_TRUE_MESSAGE(jsonBalanced(ok), apiCommandAt(i).name);

    char err[256];
    formatApiError(snap(), 0x0602, "invalid value", err, sizeof(err));
    TEST_ASSERT_TRUE_MESSAGE(jsonBalanced(err), apiCommandAt(i).name);
    TEST_ASSERT_NOT_NULL_MESSAGE(
        strstr(err, "\"error\":{\"code\":1538"), apiCommandAt(i).name);
  }
}

void runApiCommandsTests() {
  RUN_TEST(test_help_array_exact);
  RUN_TEST(test_help_full_line_renders);
  RUN_TEST(test_m4_m5_commands_absent);
  RUN_TEST(test_no_duplicate_commands);
  RUN_TEST(test_surface_envelope_sweep);
}
