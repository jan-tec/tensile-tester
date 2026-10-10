// Unit tests for the sample buffer + CSV contract (M3 FR-004/FR-005,
// TC-004/TC-005). Run: pio test -e native
//
// TC-005 "exact-once" is the load-bearing property: a `data <from>` dump
// resumed after a connection drop must replay nothing and drop nothing, so
// chunks are WHOLE LINES ONLY and *nextIndex only ever points at a line
// boundary (plan section 5, TestData.h contract).
#include <unity.h>

#include <TestData.h>

#include <cstring>
#include <string>

namespace {
TestSample sample(size_t i) {
  TestSample s;
  s.positionMm = static_cast<float>(i) * 0.25f;
  s.forceN = static_cast<float>(i) * 2.0f;
  return s;
}

// Drain a full dump through small chunk buffers (exercises the resume loop
// exactly like the serial data path does). Caller stop rule (contract):
// finish when a chunk writes 0 bytes OR no longer advances the index but
// still wrote bytes (that chunk is the END-marker-only final chunk).
std::string drain(const SampleBuffer &buf) {
  std::string out;
  char chunk[64];
  size_t next = 0;
  size_t prev = 0;
  size_t written = 0;
  do {
    prev = next;
    written = formatCsvChunk(buf, next, chunk, sizeof(chunk), &next);
    out.append(chunk, written);
  } while (written > 0 && next != prev);
  return out;
}
}  // namespace

void test_sample_is_8_bytes(void) {
  // TC-004 memory budget: 8192 samples must fit in 64 KB.
  TEST_ASSERT_EQUAL_UINT32(8, sizeof(TestSample));
}

void test_buffer_push_count_at_clear(void) {
  SampleBuffer b;
  TEST_ASSERT_EQUAL_UINT32(0, static_cast<uint32_t>(b.count()));
  TEST_ASSERT_FALSE(b.truncated());
  TEST_ASSERT_TRUE(b.push(sample(0)));
  TEST_ASSERT_TRUE(b.push(sample(1)));
  TEST_ASSERT_EQUAL_UINT32(2, static_cast<uint32_t>(b.count()));
  TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.25f, b.at(1).positionMm);
  TEST_ASSERT_FLOAT_WITHIN(1e-6f, 2.0f, b.at(1).forceN);
  b.clear();
  TEST_ASSERT_EQUAL_UINT32(0, static_cast<uint32_t>(b.count()));
}

void test_buffer_capacity_and_truncated(void) {
  SampleBuffer b;
  for (size_t i = 0; i < SampleBuffer::CAPACITY; i++) {
    TEST_ASSERT_TRUE_MESSAGE(b.push(sample(i)), "push below capacity failed");
  }
  TEST_ASSERT_FALSE(b.push(sample(9999)));  // full
  TEST_ASSERT_TRUE_MESSAGE(b.truncated(), "rejected push must flag truncated");
  b.clear();
  TEST_ASSERT_FALSE(b.truncated());
  TEST_ASSERT_TRUE(b.push(sample(0)));
}

void test_csv_header_footer_and_line_count(void) {
  SampleBuffer b;
  const size_t n = 50;
  for (size_t i = 0; i < n; i++) {
    b.push(sample(i));
  }
  const std::string csv = drain(b);
  // header exactly once, footer exactly once, one line per sample
  TEST_ASSERT_TRUE(csv.rfind("# index,displacement_mm,force_N\n", 0) == 0);
  TEST_ASSERT_TRUE(csv.find("# END samples=50 truncated=0\n") !=
                   std::string::npos);
  size_t headers = 0;
  for (size_t p = csv.find("# index,"); p != std::string::npos;
       p = csv.find("# index,", p + 1)) {
    ++headers;
  }
  TEST_ASSERT_EQUAL_UINT32(1, static_cast<uint32_t>(headers));
  size_t lines = 0;
  size_t pos = 0;
  while ((pos = csv.find("\n", pos)) != std::string::npos) {
    ++lines;
    ++pos;
  }
  TEST_ASSERT_EQUAL_UINT32(n + 2, static_cast<uint32_t>(lines));  // +hdr+END
  // a mid line has exactly the pinned shape (index, '.' decimals)
  TEST_ASSERT_NOT_NULL(std::strstr(csv.c_str(), "\n10,2.50,20.00\n"));
}

void test_csv_exact_once_resume(void) {
  // TC-005: chunks through many tiny buffers must equal one big dump, and
  // resuming mid-dump must neither repeat nor skip a line.
  SampleBuffer b;
  const size_t n = 80;
  for (size_t i = 0; i < n; i++) {
    b.push(sample(i));
  }
  const std::string whole = drain(b);

  // resume from the middle: re-drain from a resume point and splice
  char big[8192];
  size_t next = 0;
  size_t w1 = formatCsvChunk(b, 0, big, 150, &next);  // deliberately tiny
  TEST_ASSERT_TRUE(w1 > 0);
  TEST_ASSERT_TRUE(next < n);
  const std::string head(big, w1);
  char rest[8192];
  size_t next2 = next;
  size_t w2 = formatCsvChunk(b, next2, rest, sizeof(rest), &next2);
  const std::string tail(rest, w2);
  TEST_ASSERT_EQUAL_UINT32(static_cast<uint32_t>(n),
                           static_cast<uint32_t>(next2));
  TEST_ASSERT_EQUAL_STRING_MESSAGE(whole.c_str(),
                                   (head + tail).c_str(),
                                   "resume changed the byte stream");
}

void test_csv_split_line_emits_nothing(void) {
  SampleBuffer b;
  b.push(sample(0));
  b.push(sample(1));
  char buf[16];  // smaller than header + one line
  size_t next = 0;
  const size_t w = formatCsvChunk(b, 0, buf, sizeof(buf), &next);
  TEST_ASSERT_EQUAL_UINT32(0, static_cast<uint32_t>(w));
  TEST_ASSERT_EQUAL_UINT32(0, static_cast<uint32_t>(next));

  // header alone fits here, but no first line follows it: the chunk must
  // roll back, a header-only chunk would duplicate headers on resume.
  char buf2[40];
  size_t next2 = 0;
  const size_t w2 = formatCsvChunk(b, 0, buf2, sizeof(buf2), &next2);
  TEST_ASSERT_EQUAL_UINT32(0, static_cast<uint32_t>(w2));
  TEST_ASSERT_EQUAL_UINT32(0, static_cast<uint32_t>(next2));
}

void test_csv_tail_chunk_and_empty(void) {
  // empty buffer still produces a valid, complete document
  SampleBuffer empty;
  char buf[256];
  size_t next = 0;
  const size_t w = formatCsvChunk(empty, 0, buf, sizeof(buf), &next);
  const std::string doc(buf, w);
  TEST_ASSERT_EQUAL_STRING("# index,displacement_mm,force_N\n"
                           "# END samples=0 truncated=0\n",
                           doc.c_str());
  TEST_ASSERT_EQUAL_UINT32(0, static_cast<uint32_t>(next));

  // full dump in one chunk ends with the END marker (tail chunk)
  SampleBuffer b;
  for (size_t i = 0; i < 10; i++) {
    b.push(sample(i));
  }
  next = 0;
  const size_t w2 = formatCsvChunk(b, 0, buf, sizeof(buf), &next);
  TEST_ASSERT_EQUAL_UINT32(10, static_cast<uint32_t>(next));
  TEST_ASSERT_NOT_NULL(std::strstr(std::string(buf, w2).c_str(),
                                   "# END samples=10 truncated=0\n"));
}

void test_csv_truncated_flag_in_footer(void) {
  SampleBuffer b;
  for (size_t i = 0; i < SampleBuffer::CAPACITY; i++) {
    b.push(sample(i));
  }
  b.push(sample(0));  // overflow -> truncated
  char buf[64];
  size_t next = 0;
  std::string csv;
  size_t w = 0;
  do {
    w = formatCsvChunk(b, next, buf, sizeof(buf), &next);
    csv.append(buf, w);
  } while (w > 0);
  TEST_ASSERT_NOT_NULL(std::strstr(csv.c_str(),
                                   "# END samples=8192 truncated=1\n"));
}

void test_status_formatter(void) {
  SampleBuffer b;
  for (size_t i = 0; i < 5; i++) {
    b.push(sample(i));
  }
  char out[96];
  formatTestDataStatus(b, 8.0f, 4, out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING(
      "samples=5 capacity=8192 truncated=0 peak=4@8.00N", out);
  b.clear();
  formatTestDataStatus(b, 0.0f, 0, out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING("samples=0 capacity=8192 truncated=0 peak=0@0.00N",
                           out);
}

void test_csv_footer_deferred_to_next_chunk(void) {
  // The chunk where the last line would fit but line+END-marker would not:
  // the line is deferred and the function NEVER writes beyond bufSize (the
  // exact-once resume breaks if a chunk can overrun its buffer).
  SampleBuffer b;
  b.push(sample(0));
  b.push(sample(1));
  // header 32 + "0,0.00,0.00\n" 12 + "1,0.25,2.00\n" 12 + END 28 = 84
  char buf[56];
  size_t next = 0;
  const size_t w = formatCsvChunk(b, 0, buf, sizeof(buf), &next);
  TEST_ASSERT_TRUE_MESSAGE(w <= sizeof(buf), "chunk overran its buffer");
  TEST_ASSERT_EQUAL_UINT32(44, static_cast<uint32_t>(w));
  TEST_ASSERT_EQUAL_UINT32(1, static_cast<uint32_t>(next));

  // the resumed chunk then carries line 1 + END marker together
  char rest[64];
  size_t next2 = 1;
  const size_t w2 = formatCsvChunk(b, 1, rest, sizeof(rest), &next2);
  TEST_ASSERT_EQUAL_UINT32(2, static_cast<uint32_t>(next2));
  TEST_ASSERT_EQUAL_STRING("1,0.25,2.00\n# END samples=2 truncated=0\n",
                           std::string(rest, w2).c_str());
}

void runSampleDataTests(void) {
  RUN_TEST(test_sample_is_8_bytes);
  RUN_TEST(test_buffer_push_count_at_clear);
  RUN_TEST(test_buffer_capacity_and_truncated);
  RUN_TEST(test_csv_header_footer_and_line_count);
  RUN_TEST(test_csv_exact_once_resume);
  RUN_TEST(test_csv_split_line_emits_nothing);
  RUN_TEST(test_csv_footer_deferred_to_next_chunk);
  RUN_TEST(test_csv_tail_chunk_and_empty);
  RUN_TEST(test_csv_truncated_flag_in_footer);
  RUN_TEST(test_status_formatter);
}
