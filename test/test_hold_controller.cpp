// M4 T002 (#206): HoldController pure FSM transition tests (TC-001).
// Inputs are scripted per 10 Hz tick; the model must never assert
// hardware behavior (Homing.h convention).
#include <unity.h>

#include <HoldController.h>

namespace {
HoldController::Inputs baseInputs() {
  HoldController::Inputs in{};
  in.homedDone = true;
  in.forceN = 0.0f;
  in.positionMm = 0.0f;
  in.endstopBottom = false;
  in.endstopTop = false;
  in.deviceFault = false;
  in.nowMs = 1000;
  return in;
}

HoldController::Targets tensionTargets() {
  HoldController::Targets t{};
  t.setpointN = 50.0f;
  t.durationMs = 5000;
  t.tolerancePct = 5.0f;
  t.clearanceMm = 20.0f;
  return t;
}

HoldController::Output step(HoldController &c, HoldController::Inputs &in,
                            float forceN, uint32_t ms) {
  in.forceN = forceN;
  in.nowMs = ms;
  return c.tick(in);
}
}  // namespace

void test_hold_idle_stays_idle(void) {
  HoldController c;
  HoldController::Inputs in = baseInputs();
  const HoldController::Output out = c.tick(in);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(HoldController::Phase::Idle),
                        static_cast<int>(out.phase));
}

void test_hold_start_waits_for_homing(void) {
  HoldController c;
  c.start(tensionTargets());
  HoldController::Inputs in = baseInputs();
  in.homedDone = false;
  HoldController::Output out = c.tick(in);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(HoldController::Phase::Homing),
                        static_cast<int>(out.phase));
  TEST_ASSERT_FALSE(out.sample);
  TEST_ASSERT_EQUAL_INT(0, out.seekDir);
  in.homedDone = true;
  out = c.tick(in);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(HoldController::Phase::Approach),
                        static_cast<int>(out.phase));
  TEST_ASSERT_EQUAL_INT(1, out.seekDir);  // tension seeks up
}

void test_hold_tension_contact_enters_holding_jolt_free(void) {
  HoldController c;
  c.start(tensionTargets());
  HoldController::Inputs in = baseInputs();
  step(c, in, 0.0f, 1000);                       // approach
  const HoldController::Output out = step(c, in, 25.0f, 1100);  // >= 50%
  TEST_ASSERT_EQUAL_INT(static_cast<int>(HoldController::Phase::Holding),
                        static_cast<int>(out.phase));
  TEST_ASSERT_TRUE(out.holdStartTick);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, out.correctionMm);  // no jolt at entry
  TEST_ASSERT_TRUE(out.sample);
}

void test_hold_compression_transits_then_seeks_down(void) {
  HoldController c;
  HoldController::Targets t = tensionTargets();
  t.setpointN = -50.0f;
  c.start(t);
  HoldController::Inputs in = baseInputs();
  HoldController::Output out = c.tick(in);  // homing done -> approach
  TEST_ASSERT_EQUAL_INT(1, out.seekDir);    // transit UP
  in.positionMm = 10.0f;                    // below clearance
  out = step(c, in, 0.0f, 1100);
  TEST_ASSERT_EQUAL_INT(1, out.seekDir);    // still transiting
  in.positionMm = 20.0f;                    // clearance reached
  out = step(c, in, 0.0f, 1200);
  TEST_ASSERT_EQUAL_INT(-1, out.seekDir);   // now seek DOWN
}

void test_hold_transit_d26_starts_approach_not_limit(void) {
  HoldController c;
  HoldController::Targets t = tensionTargets();
  t.setpointN = -50.0f;
  c.start(t);
  HoldController::Inputs in = baseInputs();
  c.tick(in);
  in.endstopTop = true;  // D26 during transit (FR-002)
  const HoldController::Output out = step(c, in, 0.0f, 1100);
  TEST_ASSERT_EQUAL_INT(-1, out.seekDir);  // approach starts from here
  TEST_ASSERT_EQUAL_INT(static_cast<int>(HoldController::Phase::Approach),
                        static_cast<int>(out.phase));
}

void test_hold_approach_endstop_in_travel_dir_is_limit(void) {
  HoldController c;
  c.start(tensionTargets());
  HoldController::Inputs in = baseInputs();
  c.tick(in);
  in.endstopTop = true;  // tension seek up hits D26 before contact
  const HoldController::Output out = step(c, in, 0.0f, 1100);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(HoldController::Phase::Limit),
                        static_cast<int>(out.phase));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(HoldController::End::Limit),
                        static_cast<int>(out.end));
  TEST_ASSERT_EQUAL_INT(0, out.seekDir);  // motor stops
}

void test_hold_opposite_endstop_is_ignored(void) {
  // Tension hold with the carriage near the datum: E1 pressed must not
  // false-limit the upward corrections.
  HoldController c;
  c.start(tensionTargets());
  HoldController::Inputs in = baseInputs();
  step(c, in, 25.0f, 1000);  // contact -> Holding
  in.endstopBottom = true;
  const HoldController::Output out = step(c, in, 40.0f, 1100);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(HoldController::Phase::Holding),
                        static_cast<int>(out.phase));
}

void test_hold_in_band_no_correction(void) {
  HoldController c;
  c.start(tensionTargets());
  HoldController::Inputs in = baseInputs();
  step(c, in, 25.0f, 1000);
  const HoldController::Output out = step(c, in, 49.0f, 1100);  // in band
  TEST_ASSERT_EQUAL_FLOAT(0.0f, out.correctionMm);
  TEST_ASSERT_EQUAL_UINT32(1, c.inBandTicks());
}

void test_hold_underforce_corrects_outward_rate_limited(void) {
  HoldController c;
  c.start(tensionTargets());
  HoldController::Inputs in = baseInputs();
  step(c, in, 25.0f, 1000);
  // Mild underforce: gain-limited (0.002 mm/N * 20 N = 0.04 mm).
  HoldController::Output out = step(c, in, 30.0f, 1100);
  TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.04f, out.correctionMm);
  // Massive underforce: rate cap dominates (0.05 mm = 30 mm/min ceiling).
  out = step(c, in, 1.0f, 1200);
  TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.05f, out.correctionMm);
}

void test_hold_overforce_corrects_inward(void) {
  HoldController c;
  c.start(tensionTargets());
  HoldController::Inputs in = baseInputs();
  step(c, in, 25.0f, 1000);
  const HoldController::Output out = step(c, in, 60.0f, 1100);
  TEST_ASSERT_FLOAT_WITHIN(1e-6f, -0.02f, out.correctionMm);  // down
}

void test_hold_compression_correction_signs(void) {
  HoldController c;
  HoldController::Targets t = tensionTargets();
  t.setpointN = -50.0f;
  c.start(t);
  HoldController::Inputs in = baseInputs();
  // Fast-forward: transit done, contact on the downward seek.
  c.tick(in);
  in.positionMm = 20.0f;
  step(c, in, 0.0f, 1100);  // transit complete, seeking down
  const HoldController::Output out = step(c, in, -30.0f, 1200);  // contact
  TEST_ASSERT_EQUAL_INT(static_cast<int>(HoldController::Phase::Holding),
                        static_cast<int>(out.phase));
  const HoldController::Output corr = step(c, in, -30.0f, 1300);
  // Underforce in compression = press further DOWN.
  TEST_ASSERT_FLOAT_WITHIN(1e-6f, -0.04f, corr.correctionMm);
}

void test_hold_lost_after_two_underfloor_ticks(void) {
  HoldController c;
  c.start(tensionTargets());
  HoldController::Inputs in = baseInputs();
  step(c, in, 25.0f, 1000);
  HoldController::Output out = step(c, in, 10.0f, 1100);  // below floor, 1st
  TEST_ASSERT_EQUAL_INT(static_cast<int>(HoldController::Phase::Holding),
                        static_cast<int>(out.phase));
  out = step(c, in, 5.0f, 1200);  // 2nd consecutive -> Lost
  TEST_ASSERT_EQUAL_INT(static_cast<int>(HoldController::Phase::Lost),
                        static_cast<int>(out.phase));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(HoldController::End::Lost),
                        static_cast<int>(out.end));
}

void test_hold_single_underfloor_tick_recovers(void) {
  HoldController c;
  c.start(tensionTargets());
  HoldController::Inputs in = baseInputs();
  step(c, in, 25.0f, 1000);
  step(c, in, 10.0f, 1100);  // single glitch
  const HoldController::Output out = step(c, in, 49.0f, 1200);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(HoldController::Phase::Holding),
                        static_cast<int>(out.phase));
  // And a LATER single dip again starts a fresh 2-tick window.
  step(c, in, 10.0f, 1300);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(HoldController::Phase::Holding),
                        static_cast<int>(c.phase()));
}

void test_hold_duration_elapsed_ends_time(void) {
  HoldController c;
  c.start(tensionTargets());  // 5000 ms
  HoldController::Inputs in = baseInputs();
  step(c, in, 25.0f, 1000);  // contact at t=1000
  for (uint32_t t = 1100; t <= 6000; t += 100) {
    HoldController::Output out = step(c, in, 49.5f, t);
    if (out.phase == HoldController::Phase::Done) {
      TEST_ASSERT_EQUAL_INT(static_cast<int>(HoldController::End::Time),
                            static_cast<int>(out.end));
      TEST_ASSERT_EQUAL_UINT32(t, 6000);  // 1000 + 5000 exactly
      return;
    }
    TEST_ASSERT_EQUAL_INT(static_cast<int>(HoldController::Phase::Holding),
                          static_cast<int>(out.phase));
  }
  TEST_FAIL_MESSAGE("duration never elapsed");
}

void test_hold_zero_duration_runs_until_stop(void) {
  HoldController c;
  HoldController::Targets t = tensionTargets();
  t.durationMs = 0;
  c.start(t);
  HoldController::Inputs in = baseInputs();
  step(c, in, 25.0f, 1000);
  HoldController::Output out = step(c, in, 49.5f, 60 * 60 * 1000UL);  // 1 h
  TEST_ASSERT_EQUAL_INT(static_cast<int>(HoldController::Phase::Holding),
                        static_cast<int>(out.phase));
  c.requestStop();
  out = step(c, in, 49.5f, 60 * 60 * 1000UL + 100);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(HoldController::Phase::Aborted),
                        static_cast<int>(out.phase));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(HoldController::End::Stop),
                        static_cast<int>(out.end));
}

void test_hold_device_fault_aborts(void) {
  HoldController c;
  c.start(tensionTargets());
  HoldController::Inputs in = baseInputs();
  step(c, in, 25.0f, 1000);
  in.deviceFault = true;
  const HoldController::Output out = step(c, in, 49.0f, 1100);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(HoldController::Phase::Aborted),
                        static_cast<int>(out.phase));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(HoldController::End::Fault),
                        static_cast<int>(out.end));
}

void test_hold_terminal_latches_and_restart_allowed(void) {
  HoldController c;
  c.start(tensionTargets());
  HoldController::Inputs in = baseInputs();
  step(c, in, 25.0f, 1000);
  step(c, in, 5.0f, 1100);
  step(c, in, 5.0f, 1200);  // Lost
  HoldController::Output out = c.tick(in);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(HoldController::Phase::Lost),
                        static_cast<int>(out.phase));
  TEST_ASSERT_EQUAL_INT(0, out.seekDir);
  TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.0f, out.correctionMm);
  // Restart from a terminal phase is allowed (command layer owns
  // exclusivity).
  c.start(tensionTargets());
  TEST_ASSERT_EQUAL_INT(static_cast<int>(HoldController::Phase::Homing),
                        static_cast<int>(c.phase()));
}

void test_hold_nonfinite_tick_ignored(void) {
  HoldController c;
  c.start(tensionTargets());
  HoldController::Inputs in = baseInputs();
  c.tick(in);
  in.forceN = NAN;
  HoldController::Output out = c.tick(in);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(HoldController::Phase::Approach),
                        static_cast<int>(out.phase));
  TEST_ASSERT_EQUAL_INT(0, out.seekDir);  // no decision on a bad tick
}

void runHoldControllerTests() {
  RUN_TEST(test_hold_idle_stays_idle);
  RUN_TEST(test_hold_start_waits_for_homing);
  RUN_TEST(test_hold_tension_contact_enters_holding_jolt_free);
  RUN_TEST(test_hold_compression_transits_then_seeks_down);
  RUN_TEST(test_hold_transit_d26_starts_approach_not_limit);
  RUN_TEST(test_hold_approach_endstop_in_travel_dir_is_limit);
  RUN_TEST(test_hold_opposite_endstop_is_ignored);
  RUN_TEST(test_hold_in_band_no_correction);
  RUN_TEST(test_hold_underforce_corrects_outward_rate_limited);
  RUN_TEST(test_hold_overforce_corrects_inward);
  RUN_TEST(test_hold_compression_correction_signs);
  RUN_TEST(test_hold_lost_after_two_underfloor_ticks);
  RUN_TEST(test_hold_single_underfloor_tick_recovers);
  RUN_TEST(test_hold_duration_elapsed_ends_time);
  RUN_TEST(test_hold_zero_duration_runs_until_stop);
  RUN_TEST(test_hold_device_fault_aborts);
  RUN_TEST(test_hold_terminal_latches_and_restart_allowed);
  RUN_TEST(test_hold_nonfinite_tick_ignored);
}
