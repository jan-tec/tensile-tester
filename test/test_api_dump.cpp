// Unit tests for the M6 dump framing (host side, env:native).
// Run: pio test -e native
//
// #179 (M6-T003), spec FR-005, TC-004: `data` keeps the M3 CSV grammar
// byte-identical and gains JSON lines only AROUND it - one header before
// the body, one footer after. The composition pinned here is the
// contract the T006 wiring must reproduce on the wire.
#include <unity.h>

#include <cstdio>
#include <cstring>

#include <ApiResponse.h>
#include <TestData.h>

static ApiStateSnapshot idleSnapshot() {
  ApiStateSnapshot s{};
  s.posMm = 0.0f;
  s.forceN = 0.0f;
  s.cellReady = true;
  s.calValid = true;
  s.homed = true;
  s.faultCode = 0;
  s.cycleState = "Idle";
  return s;
}

// FR-005: header carries sample count, resume index, format tag - and
// is itself a valid success envelope (reply to the `data` command).
void test_dump_header_exact(void) {
  char out[256];
  formatApiDumpHeader(idleSnapshot(), 598, 500, out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING(
      "{\"ok\":true,\"data\":{\"dump\":\"begin\",\"count\":598,\"from\":500,"
      "\"format\":\"csv-m3\"},\"state\":{\"posMm\":0.000,\"forceN\":0.00,"
      "\"calValid\":true,\"cycleState\":\"Idle\",\"faultCode\":0,"
      "\"homed\":true}}",
      out);
}

// FR-005: footer carries the completion status mirroring the M3 END
// marker (truncated as 0|1 for symmetry with the CSV grammar).
void test_dump_footer_exact(void) {
  char out[256];
  formatApiDumpFooter(idleSnapshot(), 98, false, out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING(
      "{\"ok\":true,\"data\":{\"dump\":\"end\",\"sent\":98,"
      "\"truncated\":0},\"state\":{\"posMm\":0.000,\"forceN\":0.00,"
      "\"calValid\":true,\"cycleState\":\"Idle\",\"faultCode\":0,"
      "\"homed\":true}}",
      out);
  formatApiDumpFooter(idleSnapshot(), 8192, true, out, sizeof(out));
  TEST_ASSERT_NOT_NULL(strstr(out, "\"sent\":8192,\"truncated\":1"));
}

// TC-004 body byte-identity: the bytes between header-newline and
// footer are EXACTLY the standalone M3 dump (CSV header, lines, END
// marker). Framing must never touch, rewrap or reflow the body.
void test_dump_body_byte_identical(void) {
  SampleBuffer buffer;
  for (size_t i = 0; i < 5; ++i) {
    buffer.push({static_cast<float>(i) * 0.5f,
                 static_cast<float>(i) * 12.25f});
  }

  char body[512];
  size_t next = 0;
  size_t used = 0;
  while (true) {
    size_t wrote = formatCsvChunk(buffer, next, body + used,
                                  sizeof(body) - used, &next);
    used += wrote;
    if (wrote == 0 || next >= buffer.count()) {
      break;
    }
  }
  body[used] = '\0';
  TEST_ASSERT_NOT_NULL(strstr(body, "# index,displacement_mm,force_N"));
  TEST_ASSERT_NOT_NULL(strstr(body, "# END samples=5 truncated=0"));

  char header[256];
  const int hLen =
      formatApiDumpHeader(idleSnapshot(), buffer.count(), 0, header,
                          sizeof(header));
  char footer[256];
  const int fLen =
      formatApiDumpFooter(idleSnapshot(), buffer.count(), false, footer,
                          sizeof(footer));

  // Composed stream: header + "\n" + body + footer + "\n" (newlines are
  // the transport's job; the composition order is what T006 pins to).
  char stream[1024];
  const int sLen = snprintf(stream, sizeof(stream), "%s\n%s%s\n", header,
                            body, footer);
  TEST_ASSERT_TRUE(sLen > 0 && sLen < static_cast<int>(sizeof(stream)));

  const char* bodyStart = stream + hLen + 1;
  const size_t bodyLen = strlen(body);
  TEST_ASSERT_EQUAL_UINT(bodyLen, fLen != 0
                                      ? static_cast<size_t>(
                                            strstr(stream + hLen + 1,
                                                   footer) -
                                            bodyStart)
                                      : 0u);
  TEST_ASSERT_EQUAL_UINT8_ARRAY(body, bodyStart, bodyLen);
}

// snprintf truncation convention holds for the framing lines too.
void test_dump_framing_truncation_signalled(void) {
  char out[16];
  TEST_ASSERT_TRUE(
      formatApiDumpHeader(idleSnapshot(), 598, 500, out, sizeof(out)) >= 16);
  TEST_ASSERT_TRUE(
      formatApiDumpFooter(idleSnapshot(), 98, false, out, sizeof(out)) >= 16);
}

void runApiDumpTests() {
  RUN_TEST(test_dump_header_exact);
  RUN_TEST(test_dump_footer_exact);
  RUN_TEST(test_dump_body_byte_identical);
  RUN_TEST(test_dump_framing_truncation_signalled);
}
