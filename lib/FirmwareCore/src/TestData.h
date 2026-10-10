#ifndef TESTDATA_H
#define TESTDATA_H

#include <cstddef>
#include <cstdio>
#include <cstring>

// M3 FR-004 / FR-005, TC-004 / TC-005: the sample store and the CSV dump
// contract (specs/M3-tensile-test-cycle/plan.md section 5). Pure C++: the
// runner (T011) owns ONE SampleBuffer as a static (64 KB = 8192 x 8 B, the
// whole ESP budget for M3 data; never on the stack, never the heap), pushes
// raw-force samples at 10 Hz, and the serial `data [from]` handler streams
// formatCsvChunk() chunks without buffering the whole file.
//
// CSV grammar (stable contract, consumed by jan-tec's desktop tool):
//   "# index,displacement_mm,force_N"   header, first chunk only
//   "<i>,<x.x>,<y.y>"                   one line per sample, '.' decimals
//   "# END samples=<n> truncated=<0|1>" end marker, joined to the chunk that
//                                       wrote the last line
//
// Exact-once resume (TC-005): *nextIndex only ever points at a line boundary,
// and a chunk that cannot fit a WHOLE line (or, for the last line, the line
// plus the END marker) stops before it and leaves *nextIndex unchanged - a
// dropped connection can therefore resume with `data <nextIndex>` and
// replays nothing, drops nothing. Contract for callers: use a chunk buffer
// >= 96 bytes (header + longest line + END marker), stop when a call wrote
// 0 bytes or reached count() with the marker sent. A buffer too small for
// one unit always returns 0 bytes (documented livelock only on caller
// misconfiguration, never data loss).

struct TestSample {
  float positionMm;
  float forceN;
};

class SampleBuffer {
 public:
  static constexpr size_t CAPACITY = 8192;  // 64 KB with TestSample = 8 B

  // False when full; a rejected push flags `truncated` (samples were lost),
  // which the CSV footer and the `testdata` status surface (FR-006).
  bool push(const TestSample &s) {
    if (count_ >= CAPACITY) {
      truncated_ = true;
      return false;
    }
    data_[count_++] = s;
    return true;
  }

  size_t count() const { return count_; }
  bool truncated() const { return truncated_; }
  void clear() {
    count_ = 0;
    truncated_ = false;
  }
  // Caller guarantees i < count() (hot dump loop iterates by index; a
  // bounds check here would buy nothing the caller cannot guarantee).
  const TestSample &at(size_t i) const { return data_[i]; }

 private:
  TestSample data_[CAPACITY];
  size_t count_ = 0;
  bool truncated_ = false;
};

// Writes the chunk starting at sample `fromIndex` into buf (size bufSize),
// sets *nextIndex to the first sample NOT yet written, returns bytes
// written. See the contract above. fromIndex > count() clamps to count().
inline size_t formatCsvChunk(const SampleBuffer &buffer, size_t fromIndex,
                             char *buf, size_t bufSize, size_t *nextIndex) {
  const size_t n = buffer.count();
  const size_t from = fromIndex < n ? fromIndex : n;

  char endMarker[40];
  const int endLen =
      snprintf(endMarker, sizeof(endMarker), "# END samples=%u truncated=%u\n",
               static_cast<unsigned>(n), buffer.truncated() ? 1u : 0u);
  if (endLen <= 0) {
    *nextIndex = from;
    return 0;  // snprintf failure (defensive, unreachable in practice)
  }
  const size_t endSize = static_cast<size_t>(endLen);

  static const char kHeader[] = "# index,displacement_mm,force_N\n";
  const size_t headerSize = sizeof(kHeader) - 1;

  if (n == 0) {
    // An empty dump is a complete document in one chunk (header + END).
    const size_t total = headerSize + endSize;
    if (from == 0 && total <= bufSize) {
      memcpy(buf, kHeader, headerSize);
      memcpy(buf + headerSize, endMarker, endSize);
      *nextIndex = 0;
      return total;
    }
    *nextIndex = from;
    return 0;
  }
  if (from == n) {
    *nextIndex = n;  // dump already complete
    return 0;
  }

  size_t used = 0;
  if (from == 0) {
    if (headerSize > bufSize) {
      *nextIndex = from;
      return 0;
    }
    memcpy(buf, kHeader, headerSize);
    used = headerSize;
  }

  size_t next = from;
  char line[48];
  while (next < n) {
    const int lineLen = snprintf(
        line, sizeof(line), "%u,%.2f,%.2f\n", static_cast<unsigned>(next),
        static_cast<double>(buffer.at(next).positionMm),
        static_cast<double>(buffer.at(next).forceN));
    if (lineLen <= 0) {
      break;  // defensive
    }
    // whole-units rule: the last line carries the END marker, and the pair
    // fits or the line is deferred to the next chunk (never split).
    const size_t need = static_cast<size_t>(lineLen) +
                        (next + 1 == n ? endSize : 0u);
    if (used + need > bufSize) {
      break;
    }
    memcpy(buf + used, line, static_cast<size_t>(lineLen));
    used += static_cast<size_t>(lineLen);
    ++next;
    if (next == n) {
      memcpy(buf + used, endMarker, endSize);
      used += endSize;
    }
  }

  if (next == from) {
    // Nothing whole fit: undo a header-only partial write and report no
    // progress (a header-only chunk would duplicate headers on resume).
    *nextIndex = from;
    return 0;
  }
  *nextIndex = next;
  return used;
}

// `testdata` reply (FR-005), machine-readable and locale-neutral ('.'
// decimals, stable key=value grammar): the serial/Web layer wraps it in the
// locale frame, the numbers and the peak stay host-tested here.
inline size_t formatTestDataStatus(const SampleBuffer &buffer,
                                   float peakForceN, size_t peakIndex,
                                   char *out, size_t outSize) {
  return snprintf(out, outSize, "samples=%u capacity=%u truncated=%u peak=%u@%.2fN",
                  static_cast<unsigned>(buffer.count()),
                  static_cast<unsigned>(SampleBuffer::CAPACITY),
                  buffer.truncated() ? 1u : 0u,
                  static_cast<unsigned>(peakIndex),
                  static_cast<double>(peakForceN));
}

#endif  // TESTDATA_H
