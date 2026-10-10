#ifndef TESTCONFIG_H
#define TESTCONFIG_H

#include <cmath>
#include <cstdint>
#include <algorithm>

// M3 FR-005: the six operator knobs of the test cycle, persisted by
// src/Settings under NVS namespace `testmachine` (test_speed, test_arm,
// test_drop, home_clear, home_seek, home_tout). Pure data + the single
// validation boundary: every value that comes out of NVS or the command
// parser flows through clampTestConfig() exactly once, so everything
// downstream (homing model, runner, CSV) may trust the config completely.
//
// Rules (pinned by test/test_config.cpp, plan "Config model"):
//   - speed     -> [0.5, 60] mm/min (the shared feed range)
//   - arm       -> (0, 4903] N; the interval is OPEN at zero (0 N cannot
//                  arm the break detector) so 0/negative fall back to the
//                  default instead of clamping to an unusable boundary
//   - drop      -> [10, 90] %
//   - clearance -> [0.5, 10] mm
//   - seek      -> 0 means "follow the test speed" and is PRESERVED; a
//                  nonzero seek clamps to the feed range
//   - timeout   -> [1 s, 300 s] in ms
//   - any non-finite float -> that field's default (NVS can hold anything)
struct TestConfig {
  float testSpeedMmPerMin;
  float armForceN;
  float dropPercent;
  float homingClearanceMm;
  float homingSeekMmPerMin;  // 0 = follow testSpeedMmPerMin (FR-005)
  uint32_t homingTimeoutMs;
};

inline TestConfig defaultTestConfig() {
  TestConfig c;
  c.testSpeedMmPerMin = 12.0f;
  c.armForceN = 50.0f;
  c.dropPercent = 50.0f;
  c.homingClearanceMm = 2.0f;
  c.homingSeekMmPerMin = 0.0f;  // follow the test speed
  c.homingTimeoutMs = 30000;
  return c;
}

// Non-finite -> default, otherwise clamp to [lo, hi].
inline float clampField(float value, float lo, float hi, float fallback) {
  if (!std::isfinite(value)) {
    return fallback;
  }
  return std::max(lo, std::min(hi, value));
}

inline TestConfig clampTestConfig(TestConfig c) {
  const TestConfig d = defaultTestConfig();
  c.testSpeedMmPerMin =
      clampField(c.testSpeedMmPerMin, 0.5f, 60.0f, d.testSpeedMmPerMin);
  // (0, 4903] is open at zero: anything not strictly positive (incl. NaN)
  // falls back to the default - clamping 0 to a hair above it would be an
  // unusable arm force.
  c.armForceN = (!std::isfinite(c.armForceN) || c.armForceN <= 0.0f)
                    ? d.armForceN
                    : std::min(4903.0f, c.armForceN);
  c.dropPercent = clampField(c.dropPercent, 10.0f, 90.0f, d.dropPercent);
  c.homingClearanceMm = clampField(c.homingClearanceMm, 0.5f, 10.0f,
                                   d.homingClearanceMm);
  // 0 is a legitimate stored value ("follow the test speed"), never clamp
  // it away; non-finite degenerates to the same follow default.
  if (!std::isfinite(c.homingSeekMmPerMin)) {
    c.homingSeekMmPerMin = 0.0f;
  } else if (c.homingSeekMmPerMin != 0.0f) {
    c.homingSeekMmPerMin = std::max(0.5f, std::min(60.0f, c.homingSeekMmPerMin));
  }
  c.homingTimeoutMs = std::max<uint32_t>(1000, std::min<uint32_t>(
                                                   300000, c.homingTimeoutMs));
  return c;
}

// Resolution of the follow-me rule for the homing seek stage: a config
// kept at seek == 0 runs the seek at the (already clamped) test speed.
inline float homingSeekSpeed(const TestConfig &c) {
  return c.homingSeekMmPerMin == 0.0f ? c.testSpeedMmPerMin
                                      : c.homingSeekMmPerMin;
}

#endif  // TESTCONFIG_H
