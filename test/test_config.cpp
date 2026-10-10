// Unit tests for the pure test-cycle configuration (M3 FR-005).
// Run: pio test -e native
//
// TestConfig holds the six operator knobs persisted by Settings (NVS
// namespace testmachine). clampTestConfig() is the single validation
// boundary: NVS values and parser values both flow through it, so the
// runner may trust its input completely afterwards.
#include <unity.h>

#include <TestConfig.h>

#include <cmath>
#include <cstring>

void test_default_config(void) {
  const TestConfig c = defaultTestConfig();
  TEST_ASSERT_FLOAT_WITHIN(1e-6f, 12.0f, c.testSpeedMmPerMin);
  TEST_ASSERT_FLOAT_WITHIN(1e-6f, 50.0f, c.armForceN);
  TEST_ASSERT_FLOAT_WITHIN(1e-6f, 50.0f, c.dropPercent);
  TEST_ASSERT_FLOAT_WITHIN(1e-6f, 2.0f, c.homingClearanceMm);
  TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.0f, c.homingSeekMmPerMin);
  TEST_ASSERT_EQUAL_UINT32(30000, c.homingTimeoutMs);
  // default clamps to itself
  const TestConfig r = clampTestConfig(c);
  TEST_ASSERT_TRUE(memcmp(&r, &c, sizeof(c)) == 0);
}

void test_clamp_speed(void) {
  TestConfig c = defaultTestConfig();
  c.testSpeedMmPerMin = -5.0f;
  TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.5f, clampTestConfig(c).testSpeedMmPerMin);
  c.testSpeedMmPerMin = 500.0f;
  TEST_ASSERT_FLOAT_WITHIN(1e-6f, 60.0f, clampTestConfig(c).testSpeedMmPerMin);
}

void test_clamp_arm_open_interval(void) {
  TestConfig c = defaultTestConfig();
  c.armForceN = 0.0f;  // (0, 4903] is open at zero: 0 is not a valid arm
  TEST_ASSERT_FLOAT_WITHIN(1e-6f, 50.0f, clampTestConfig(c).armForceN);
  c.armForceN = -3.0f;
  TEST_ASSERT_FLOAT_WITHIN(1e-6f, 50.0f, clampTestConfig(c).armForceN);
  c.armForceN = 9999.0f;  // above the cell's measurable span
  TEST_ASSERT_FLOAT_WITHIN(1e-6f, 4903.0f, clampTestConfig(c).armForceN);
}

void test_clamp_drop_clearance(void) {
  TestConfig c = defaultTestConfig();
  c.dropPercent = 5.0f;
  TEST_ASSERT_FLOAT_WITHIN(1e-6f, 10.0f, clampTestConfig(c).dropPercent);
  c.dropPercent = 95.0f;
  TEST_ASSERT_FLOAT_WITHIN(1e-6f, 90.0f, clampTestConfig(c).dropPercent);
  c.homingClearanceMm = 0.1f;
  TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.5f, clampTestConfig(c).homingClearanceMm);
  c.homingClearanceMm = 50.0f;
  TEST_ASSERT_FLOAT_WITHIN(1e-6f, 10.0f, clampTestConfig(c).homingClearanceMm);
}

void test_seek_zero_follows_test_speed(void) {
  // FR-005: the seek speed is not an independent knob; 0 (stored or default)
  // means "follow the test speed" and must survive clamping untouched.
  TestConfig c = defaultTestConfig();
  c.homingSeekMmPerMin = 0.0f;
  TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.0f, clampTestConfig(c).homingSeekMmPerMin);
  TEST_ASSERT_FLOAT_WITHIN(1e-6f, 12.0f, homingSeekSpeed(c));
  c.testSpeedMmPerMin = 30.0f;
  TEST_ASSERT_FLOAT_WITHIN(1e-6f, 30.0f, homingSeekSpeed(c));
  // a nonzero seek speed is clamped to the feed range
  c.homingSeekMmPerMin = 500.0f;
  TEST_ASSERT_FLOAT_WITHIN(1e-6f, 60.0f, clampTestConfig(c).homingSeekMmPerMin);
}

void test_clamp_timeout(void) {
  TestConfig c = defaultTestConfig();
  c.homingTimeoutMs = 0;  // 1 s floor
  TEST_ASSERT_EQUAL_UINT32(1000, clampTestConfig(c).homingTimeoutMs);
  c.homingTimeoutMs = 999999;  // 300 s ceiling
  TEST_ASSERT_EQUAL_UINT32(300000, clampTestConfig(c).homingTimeoutMs);
}

void test_nonfinite_becomes_field_default(void) {
  const TestConfig d = defaultTestConfig();
  TestConfig c = defaultTestConfig();
  const float nan = NAN;
  const float inf = INFINITY;
  c.testSpeedMmPerMin = nan;
  c.armForceN = inf;
  c.dropPercent = -nan;  // sign of NaN must not escape the rule
  c.homingClearanceMm = inf;
  TEST_ASSERT_FLOAT_WITHIN(1e-6f, d.testSpeedMmPerMin,
                           clampTestConfig(c).testSpeedMmPerMin);
  TEST_ASSERT_FLOAT_WITHIN(1e-6f, d.armForceN, clampTestConfig(c).armForceN);
  TEST_ASSERT_FLOAT_WITHIN(1e-6f, d.dropPercent, clampTestConfig(c).dropPercent);
  TEST_ASSERT_FLOAT_WITHIN(
      1e-6f, d.homingClearanceMm, clampTestConfig(c).homingClearanceMm);
}

void runConfigTests(void) {
  RUN_TEST(test_default_config);
  RUN_TEST(test_clamp_speed);
  RUN_TEST(test_clamp_arm_open_interval);
  RUN_TEST(test_clamp_drop_clearance);
  RUN_TEST(test_seek_zero_follows_test_speed);
  RUN_TEST(test_clamp_timeout);
  RUN_TEST(test_nonfinite_becomes_field_default);
}
