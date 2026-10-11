// #158: plain (non-runner) move endstop guard - pure verdict tests.
// The wiring (isRunning/moveDirection/forceStop, debounced levels) is
// target-verified; this pins the decision table.
#include <unity.h>

#include "ManualMoveGuard.h"

void test_mm_guard_down_move_stops_on_bottom(void) {
  TEST_ASSERT_TRUE(manualMoveBlockedByEndstop(-1, true, false));
}

void test_mm_guard_down_move_ignores_top_level(void) {
  TEST_ASSERT_FALSE(manualMoveBlockedByEndstop(-1, false, true));
}

void test_mm_guard_up_move_stops_on_top(void) {
  TEST_ASSERT_TRUE(manualMoveBlockedByEndstop(1, false, true));
}

void test_mm_guard_up_move_ignores_bottom_level(void) {
  // The datum-adjacent case: carriage rests with E1 pressed (homed at
  // the switch) - an upward clearing move must NOT be stopped by it.
  TEST_ASSERT_FALSE(manualMoveBlockedByEndstop(1, true, false));
}

void test_mm_guard_no_direction_never_stops(void) {
  TEST_ASSERT_FALSE(manualMoveBlockedByEndstop(0, true, true));
}

void test_mm_guard_clear_switches_never_stop(void) {
  TEST_ASSERT_FALSE(manualMoveBlockedByEndstop(-1, false, false));
  TEST_ASSERT_FALSE(manualMoveBlockedByEndstop(1, false, false));
}

void runManualMoveGuardTests() {
  RUN_TEST(test_mm_guard_down_move_stops_on_bottom);
  RUN_TEST(test_mm_guard_down_move_ignores_top_level);
  RUN_TEST(test_mm_guard_up_move_stops_on_top);
  RUN_TEST(test_mm_guard_up_move_ignores_bottom_level);
  RUN_TEST(test_mm_guard_no_direction_never_stops);
  RUN_TEST(test_mm_guard_clear_switches_never_stop);
}
