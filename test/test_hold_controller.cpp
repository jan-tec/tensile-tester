// M4 T002 (#206): HoldController pure FSM transition tests (TC-001).
// Inputs are scripted per 10 Hz tick; the model must never assert
// hardware behavior (Homing.h convention).
#include <unity.h>

#include <HoldController.h>

#include <cstdio>

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

// TC-005: accounting on a scripted in-band hold.
void test_hold_report_in_band_run(void) {
  HoldController c;
  c.start(tensionTargets());  // 50 N, 5%, 5000 ms
  HoldController::Inputs in = baseInputs();
  step(c, in, 25.0f, 1000);  // contact
  for (uint32_t t = 1100, i = 0; t < 6000; t += 100, ++i) {
    step(c, in, 50.0f + (i % 2 ? 1.0f : -1.0f), t);  // +/-1 N = in band
  }
  const HoldController::Report r = c.report();
  TEST_ASSERT_EQUAL_UINT32(49, r.holdTicks);  // 1100..5900 entered at 6000
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 2449.0f / 49.0f, r.meanForceN);  // 25x49,24x51
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 2.0f, r.maxDevPct);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 100.0f, r.inBandPct);
}

// TC-005: a drifting run spends part of the ticks out of band and the
// creep travel reflects the net carriage motion.
void test_hold_report_drifting_run(void) {
  HoldController c;
  c.start(tensionTargets());
  HoldController::Inputs in = baseInputs();
  in.positionMm = 10.0f;  // staged BEFORE contact: this is the hold start
  step(c, in, 25.0f, 1000);
  // 10 in-band ticks then the sample yields: force sinks, loop chases.
  for (uint32_t t = 1100; t < 2100; t += 100) {
    step(c, in, 50.0f, t);
  }
  for (uint32_t t = 2100; t < 5100; t += 100) {
    in.positionMm += 0.05f;  // the applied outward corrections travel
    step(c, in, 35.0f, t);   // stubbornly out of band (30% dev)
  }
  in.positionMm += 0.05f;
  step(c, in, 35.0f, 5100);  // ...until the duration ends
  const HoldController::Report r = c.report();
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 1585.0f / 41.0f, r.meanForceN);  // 10x50+31x35
  TEST_ASSERT_FLOAT_WITHIN(0.1f, 30.0f, r.maxDevPct);              // |35-50|/50
  TEST_ASSERT_FLOAT_WITHIN(0.1f, 10.0f * 100.0f / 41.0f, r.inBandPct);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 31 * 0.05f, r.creepMm);  // 30 loop + 1 final
}

// TC-002: first-peak overshoot bound on the pure closed loop. Sample
// modeled as a linear spring; worst documented envelope: the +10% bound
// (spec) holds for stiffness <= 100 N/mm at setpoint 50 N (k_bound =
// setpoint * 10% / rate 0.05 mm); stiffer fixtures need bench tuning
// (approach threshold or choltol), documented in the plan risk table.
void test_hold_overshoot_bound_spring_sweep(void) {
  const float stiffness[] = {5.0f, 25.0f, 50.0f, 100.0f};
  for (float k : stiffness) {
    HoldController c;
    c.start(tensionTargets());  // 50 N, tol 5%
    HoldController::Inputs in = baseInputs();
    in.positionMm = 0.0f;
    float force = 0.0f;
    float peak = 0.0f;
    uint32_t contactPos = 0;
    // Approach at 30 mm/min = 0.05 mm/tick (the machine cap): the spring
    // force rises k * 0.05 per tick until contact threshold.
    for (uint32_t t = 1000; t < 20000; t += 100) {
      in.forceN = force;
      in.positionMm += 0.05f;
      in.nowMs = t;
      const HoldController::Output out = c.tick(in);
      if (out.phase == HoldController::Phase::Holding) {
        contactPos = t;
        break;
      }
      if (out.phase != HoldController::Phase::Approach ||
          out.seekDir != 1) {
        TEST_FAIL_MESSAGE("approach never contacted");
      }
      force = k * in.positionMm;  // spring: carriage climbing the sample
    }
    // Closed loop: apply the emitted corrections to the spring.
    for (uint32_t t = contactPos; t < contactPos + 20000; t += 100) {
      in.nowMs = t;
      const HoldController::Output out = c.tick(in);
      in.positionMm += out.correctionMm;
      force = k * in.positionMm;
      in.forceN = force;
      if (force > peak) {
        peak = force;
      }
      if (out.phase != HoldController::Phase::Holding &&
          out.phase != HoldController::Phase::Done) {
        break;  // lost/limit: out of the overshoot question's scope
      }
    }
    char msg[64];
    snprintf(msg, sizeof(msg), "k=%.0f peak=%.2f", k, peak);
    TEST_ASSERT_LESS_OR_EQUAL_FLOAT_MESSAGE(55.0f, peak, msg);  // +10%
  }
}

// #220: a probe that tears DURING APPROACH (below the 50% threshold)
// must end Lost, not seek forever - contact evidence + collapse, same
// 2-tick confirm as FR-004.
void test_hold_approach_probe_failure_ends_lost(void) {
  HoldController c;
  c.start(tensionTargets());  // 50 N -> threshold 25 N
  HoldController::Inputs in = baseInputs();
  step(c, in, 0.0f, 1000);   // seek, no contact
  HoldController::Output out = step(c, in, 8.0f, 1100);  // contact evidence (>= max(1, 5%))
  TEST_ASSERT_EQUAL_INT(static_cast<int>(HoldController::Phase::Approach),
                        static_cast<int>(out.phase));  // 8 < 25: still seeking
  out = step(c, in, 2.0f, 1200);  // collapse below 50% of peak (8): 1st tick
  TEST_ASSERT_EQUAL_INT(static_cast<int>(HoldController::Phase::Approach),
                        static_cast<int>(out.phase));
  out = step(c, in, 1.5f, 1300);  // 2nd consecutive -> Lost
  TEST_ASSERT_EQUAL_INT(static_cast<int>(HoldController::Phase::Lost),
                        static_cast<int>(out.phase));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(HoldController::End::Lost),
                        static_cast<int>(out.end));
}

void test_hold_approach_single_dip_recovers(void) {
  HoldController c;
  c.start(tensionTargets());
  HoldController::Inputs in = baseInputs();
  step(c, in, 0.0f, 1000);
  step(c, in, 8.0f, 1100);   // contact evidence
  step(c, in, 2.0f, 1200);   // single glitch below half-peak
  const HoldController::Output out = step(c, in, 30.0f, 1300);  // recovers, contacts
  TEST_ASSERT_EQUAL_INT(static_cast<int>(HoldController::Phase::Holding),
                        static_cast<int>(out.phase));
}

void test_hold_approach_noise_below_evidence_never_latches_lost(void) {
  // No contact evidence (all samples < 1 N = the floor for a 50 N hold
  // is max(1, 5%) -> 2.5? see impl: evidence = max(1 N, 5% |sp|)): the
  // collapse rule must stay unarmed; only the D26 bound ends the seek.
  HoldController c;
  c.start(tensionTargets());
  HoldController::Inputs in = baseInputs();
  step(c, in, 0.0f, 1000);
  step(c, in, 0.8f, 1100);  // below evidence floor
  HoldController::Output out = step(c, in, 0.3f, 1200);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(HoldController::Phase::Approach),
                        static_cast<int>(out.phase));
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
  RUN_TEST(test_hold_approach_probe_failure_ends_lost);
  RUN_TEST(test_hold_approach_single_dip_recovers);
  RUN_TEST(test_hold_approach_noise_below_evidence_never_latches_lost);
  RUN_TEST(test_hold_report_in_band_run);
  RUN_TEST(test_hold_report_drifting_run);
  RUN_TEST(test_hold_overshoot_bound_spring_sweep);
}
