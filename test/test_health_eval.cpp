// Unit tests for the pure health evaluation helpers (host side, env:native).
// Run: pio test -e native
//
// M1 FR-005 / FR-006, TC-002 (RED-first): power rail threshold evaluation
// (FAIL < 4.50 V, WARN < 4.75 V), hysteresis at the FAIL boundary, the
// >= 5 consecutive out-of-band sample debounce before a runtime FAIL, and
// the HX711 500 ms variance window (10 Hz sampling = 50 samples).
#include <unity.h>

#include <FirmwareCore.h>

// --- FR-005 thresholds (single-shot, used by the boot POWER check) --------

void test_power_status_boundaries(void) {
  TEST_ASSERT_EQUAL(static_cast<int>(PowerStatus::Fail),
                    static_cast<int>(powerStatusForVoltage(4.49f)));
  // 4.50 V is NOT below the fail threshold.
  TEST_ASSERT_EQUAL(static_cast<int>(PowerStatus::Warn),
                    static_cast<int>(powerStatusForVoltage(4.50f)));
  TEST_ASSERT_EQUAL(static_cast<int>(PowerStatus::Warn),
                    static_cast<int>(powerStatusForVoltage(4.74f)));
  TEST_ASSERT_EQUAL(static_cast<int>(PowerStatus::OK),
                    static_cast<int>(powerStatusForVoltage(4.75f)));
  TEST_ASSERT_EQUAL(static_cast<int>(PowerStatus::OK),
                    static_cast<int>(powerStatusForVoltage(5.0f)));
}

// --- FR-005 NOT_WIRED detection (D4: pulldown, absent divider) -------------

// An unwired sense input (GPIO34 INPUT_PULLDOWN) reads ~0 V rail-equivalent.
// A live 5 V rail can never be below POWER_NOT_WIRED_VOLTS, so readings
// under it mean absent, not low (D3: info, never FAIL).
void test_power_not_wired_boundaries(void) {
  TEST_ASSERT_TRUE(powerNotWired(0.0f));
  TEST_ASSERT_TRUE(powerNotWired(0.99f));
  // 1.00 V rail-equivalent is NOT below the not-wired threshold.
  TEST_ASSERT_FALSE(powerNotWired(1.00f));
  TEST_ASSERT_FALSE(powerNotWired(2.50f));
  TEST_ASSERT_FALSE(powerNotWired(5.0f));
}

// --- FR-005 runtime monitor: 5-sample FAIL debounce ------------------------

void test_power_monitor_debounce_four_is_not_fail(void) {
  PowerMonitor monitor;
  for (int i = 0; i < 4; i++) {
    monitor.update(4.40f);
    TEST_ASSERT_NOT_EQUAL(static_cast<int>(PowerStatus::Fail),
                          static_cast<int>(monitor.state()));
  }
}

void test_power_monitor_five_consecutive_is_fail(void) {
  PowerMonitor monitor;
  for (int i = 0; i < 5; i++) {
    monitor.update(4.40f);
  }
  TEST_ASSERT_EQUAL(static_cast<int>(PowerStatus::Fail),
                    static_cast<int>(monitor.state()));
}

void test_power_monitor_single_good_sample_resets_streak(void) {
  PowerMonitor monitor;
  for (int i = 0; i < 4; i++) {
    monitor.update(4.40f);
  }
  monitor.update(4.90f);  // clears the streak
  for (int i = 0; i < 4; i++) {
    monitor.update(4.40f);
    TEST_ASSERT_NOT_EQUAL(static_cast<int>(PowerStatus::Fail),
                          static_cast<int>(monitor.state()));
  }
}

// --- FR-005 hysteresis: band [4.50, 4.55) does NOT clear the streak --------

void test_power_monitor_hysteresis_band_keeps_streak(void) {
  PowerMonitor monitor;
  for (int i = 0; i < 4; i++) {
    monitor.update(4.40f);
  }
  monitor.update(4.52f);  // inside the hysteresis band: streak preserved
  monitor.update(4.40f);  // 5th out-of-band sample overall
  TEST_ASSERT_EQUAL(static_cast<int>(PowerStatus::Fail),
                    static_cast<int>(monitor.state()));
}

void test_power_monitor_hysteresis_defers_recovery(void) {
  PowerMonitor monitor;
  for (int i = 0; i < 5; i++) {
    monitor.update(4.40f);
  }
  TEST_ASSERT_EQUAL(static_cast<int>(PowerStatus::Fail),
                    static_cast<int>(monitor.state()));
  monitor.update(4.52f);  // inside band: still Fail
  TEST_ASSERT_EQUAL(static_cast<int>(PowerStatus::Fail),
                    static_cast<int>(monitor.state()));
}

// --- FR-005 recovery --------------------------------------------------------

void test_power_monitor_recovery_to_ok(void) {
  PowerMonitor monitor;
  for (int i = 0; i < 5; i++) {
    monitor.update(4.40f);
  }
  monitor.update(4.90f);  // above WARN + hysteresis
  TEST_ASSERT_EQUAL(static_cast<int>(PowerStatus::OK),
                    static_cast<int>(monitor.state()));
  // Streak is cleared after recovery.
  for (int i = 0; i < 4; i++) {
    monitor.update(4.40f);
    TEST_ASSERT_NOT_EQUAL(static_cast<int>(PowerStatus::Fail),
                          static_cast<int>(monitor.state()));
  }
}

void test_power_monitor_recovery_to_warn(void) {
  PowerMonitor monitor;
  for (int i = 0; i < 5; i++) {
    monitor.update(4.40f);
  }
  monitor.update(4.60f);  // above FAIL + hysteresis, below WARN + hysteresis
  TEST_ASSERT_EQUAL(static_cast<int>(PowerStatus::Warn),
                    static_cast<int>(monitor.state()));
}

void test_power_monitor_normal_operation(void) {
  PowerMonitor monitor;
  for (int i = 0; i < 10; i++) {
    monitor.update(5.0f);
  }
  TEST_ASSERT_EQUAL(static_cast<int>(PowerStatus::OK),
                    static_cast<int>(monitor.state()));
}

// --- FR-006 HX711 variance window (500 ms @ 10 Hz) --------------------------

void test_sample_window_spread(void) {
  SampleWindow window;
  TEST_ASSERT_EQUAL_UINT(0, window.size());
  TEST_ASSERT_EQUAL_FLOAT(0.0f, window.spread());

  window.push(1000.0f);
  window.push(1010.0f);
  window.push(995.0f);
  TEST_ASSERT_EQUAL_UINT(3, window.size());
  TEST_ASSERT_EQUAL_FLOAT(15.0f, window.spread());  // 1010 - 995
}

void test_sample_window_single_sample(void) {
  SampleWindow window;
  window.push(1000.0f);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, window.spread());
}

void test_sample_window_wraps_at_capacity(void) {
  SampleWindow window;
  for (int i = 0; i < SampleWindow::CAPACITY; i++) {
    window.push(1000.0f + static_cast<float>(i));
  }
  TEST_ASSERT_TRUE(window.full());
  // Pushing while full overwrites exactly one slot: the oldest (1000)
  // drops out; spread = 1049 - 1001 = 48.
  window.push(1049.0f);
  TEST_ASSERT_EQUAL_FLOAT(48.0f, window.spread());
}

void test_sample_window_no_variation_detection(void) {
  // Dead/shorted cell: constant reading -> spread below the threshold.
  SampleWindow window;
  for (int i = 0; i < SampleWindow::CAPACITY; i++) {
    window.push(100000.0f);
  }
  TEST_ASSERT_TRUE(window.spread() < HX711_NO_VARIATION_SPREAD);

  // A live cell moves by more than the threshold.
  for (int i = 0; i < SampleWindow::CAPACITY; i++) {
    window.push(100000.0f + static_cast<float>(i) * 2.0f);
  }
  TEST_ASSERT_TRUE(window.spread() >= HX711_NO_VARIATION_SPREAD);
}

void runHealthEvalTests(void) {
  RUN_TEST(test_power_status_boundaries);
  RUN_TEST(test_power_not_wired_boundaries);
  RUN_TEST(test_power_monitor_debounce_four_is_not_fail);
  RUN_TEST(test_power_monitor_five_consecutive_is_fail);
  RUN_TEST(test_power_monitor_single_good_sample_resets_streak);
  RUN_TEST(test_power_monitor_hysteresis_band_keeps_streak);
  RUN_TEST(test_power_monitor_hysteresis_defers_recovery);
  RUN_TEST(test_power_monitor_recovery_to_ok);
  RUN_TEST(test_power_monitor_recovery_to_warn);
  RUN_TEST(test_power_monitor_normal_operation);
  RUN_TEST(test_sample_window_spread);
  RUN_TEST(test_sample_window_single_sample);
  RUN_TEST(test_sample_window_wraps_at_capacity);
  RUN_TEST(test_sample_window_no_variation_detection);
}
