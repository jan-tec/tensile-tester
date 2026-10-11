// Unit tests for FirmwareCore motion math (host side, env:native).
// Run: pio test -e native
//
// All test files in test/ are linked into ONE binary (PlatformIO default),
// so each file exposes a run*Tests() function and test/test_main.cpp holds
// the single main()/setUp()/tearDown().
#include <unity.h>

#include <FirmwareCore.h>

void test_output_steps_per_revolution(void) {
  // 200 steps/rev x 13.73 gear x 8 microsteps = 21968 steps/spindle rev.
  TEST_ASSERT_FLOAT_WITHIN(0.5f, 21968.0f, OUTPUT_STEPS_PER_REV);
}

void test_full_spindle_revolution_in_mm_and_steps(void) {
  TEST_ASSERT_EQUAL(21968, spindleMmToSteps(2.0f));
  TEST_ASSERT_EQUAL(21968, outputTurnsToSteps(1.0f));
  TEST_ASSERT_EQUAL(0, spindleMmToSteps(0.0f));
}

void test_half_revolution(void) {
  TEST_ASSERT_EQUAL(10984, spindleMmToSteps(1.0f));
}

void test_negative_direction(void) {
  TEST_ASSERT_EQUAL(-10984, spindleMmToSteps(-1.0f));
  TEST_ASSERT_EQUAL(-21968, spindleMmToSteps(-2.0f));
}

void test_fractional_mm_rounds_to_nearest_step(void) {
  // 0.5 mm = quarter spindle rev = 5492 steps exactly.
  TEST_ASSERT_EQUAL(5492, spindleMmToSteps(0.5f));
}

void test_steps_to_spindle_mm(void) {
  // Inverse of spindleMmToSteps: one spindle rev = 21968 steps = 2 mm.
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 2.0f, spindleMmFromSteps(21968));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, spindleMmFromSteps(10984));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, spindleMmFromSteps(0));
}

void test_rpm_to_steps_per_second(void) {
  // 60 rpm at the spindle = one spindle revolution per second.
  TEST_ASSERT_FLOAT_WITHIN(0.5f, OUTPUT_STEPS_PER_REV,
                           outputRpmToStepsPerSecond(60.0f));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, outputRpmToStepsPerSecond(0.0f));
}

void test_feed_conversions(void) {
  // 2 mm per spindle rev: 5 rpm = 10 mm/min.
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 10.0f, rpmToFeedMmPerMin(5.0f));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 5.0f, feedMmPerMinToRpm(10.0f));
  // Round trip is stable: 6 rpm -> 12 mm/min -> 6 rpm.
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 6.0f,
                           feedMmPerMinToRpm(rpmToFeedMmPerMin(6.0f)));
}

void test_clamp_feed_below_min(void) {
  TEST_ASSERT_FLOAT_WITHIN(0.0f, MIN_MOVE_MM_PER_MIN,
                           clampFeedMmPerMin(0.0f));
  TEST_ASSERT_FLOAT_WITHIN(0.0f, MIN_MOVE_MM_PER_MIN,
                           clampFeedMmPerMin(0.49f));
}

void test_clamp_feed_above_max(void) {
  TEST_ASSERT_FLOAT_WITHIN(0.0f, MAX_MOVE_MM_PER_MIN,
                           clampFeedMmPerMin(30.01f));
  TEST_ASSERT_FLOAT_WITHIN(0.0f, MAX_MOVE_MM_PER_MIN,
                           clampFeedMmPerMin(1000.0f));
}

void test_clamp_feed_inside_range_is_unchanged(void) {
  TEST_ASSERT_FLOAT_WITHIN(0.0f, 12.0f, clampFeedMmPerMin(12.0f));
}

void runGearMathTests() {
  RUN_TEST(test_output_steps_per_revolution);
  RUN_TEST(test_full_spindle_revolution_in_mm_and_steps);
  RUN_TEST(test_half_revolution);
  RUN_TEST(test_negative_direction);
  RUN_TEST(test_fractional_mm_rounds_to_nearest_step);
  RUN_TEST(test_steps_to_spindle_mm);
  RUN_TEST(test_rpm_to_steps_per_second);
  RUN_TEST(test_feed_conversions);
  RUN_TEST(test_clamp_feed_below_min);
  RUN_TEST(test_clamp_feed_above_max);
  RUN_TEST(test_clamp_feed_inside_range_is_unchanged);
}
