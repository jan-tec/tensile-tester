// Unit tests for the pure staged homing model (host side, env:native).
// Run: pio test -e native
//
// M3 FR-001/FR-008, TC-006/TC-007: bottom homing BackOff -> SeekDown ->
// FinalSeek -> Done sets the datum at the stage-2 trigger; top seek
// SeekUp -> BackOffDown -> Done NEVER sets a datum. Contract:
// specs/M3-tensile-test-cycle/plan.md section 5.
//
// Pinned semantics the contract leaves open (documented in Homing.h):
//   - elapsedMs is a monotonic injected ms counter; only differences are
//     used. Each seek phase (SeekDown/FinalSeek/SeekUp) owns one timeout
//     window measured from its entry; BackOff and BackOffDown never time out.
//   - expiry is strict: delta == timeoutMs has NOT expired, delta + 1 has.
//   - a guarded distance D counts as consumed once the measured travel
//     reaches D - oneStepMm (one motor step of tolerance absorbs the step
//     quantisation of the injected positions; datum identity between the
//     stage-1 and stage-2 trigger of an ideal run is then exact).
//   - trigger checks run before the timeout check in the same tick: a switch
//     trigger at the same tick the window expires is the good outcome.
//   - FinalSeek internally back-offs up (full speed) and then re-seeks down
//     (speedFraction = finalSpeedFraction); a still-pressed bottom switch
//     during the internal back-off is bounce (D4 lives in src/Endstops) and
//     is ignored; the first judged re-trigger is on the down sub-stage.
//   - SeekUp has NO instant-trigger guard (immediate D26 hit is legal).
//   - start()/startTopSeek() are accepted only from Idle or Done (idempotent
//     re-homing); startFromTop() only from BackOff. A Fault latches until
//     configure(): recovery is reset/reboot (M1 FR-009).
//   - configure() re-parameterises AND resets to Idle (like BreakDetector).
//   - tick() with a non-finite position ignores the whole tick (NaN feedback
//     must never trigger a decision, good or bad).
//   - backOffCappedByTopLimit() is set only by startFromTop(), survives to
//     Done, and is cleared by the next start()/startTopSeek()/configure().
//
// All injected positions are exactly representable floats; the tolerance
// boundary is pinched from both sides (see the BreakDetect history rule on
// float boundaries). Default test parameters: clearance 2 mm, final speed
// 0.25, timeout 30000 ms, oneStep 0.25 mm -> consumed threshold 1.75 mm.
#include <cmath>

#include <unity.h>

#include <Homing.h>

namespace {

HomingModel configured(float clearanceMm = 2.0f, float finalFraction = 0.25f,
                       uint32_t timeoutMs = 30000, float oneStepMm = 0.25f) {
  HomingModel m;
  m.configure(clearanceMm, finalFraction, timeoutMs, oneStepMm);
  return m;
}

// Default-parameter prefixes. Each helper advances one stage boundary:
// entry at 10.0@0, back-off consumed at 11.75@50 (travel 1.75 = clearance
// - oneStep, the tolerance boundary).
void seekDownReached(HomingModel& m) {
  m.start();
  m.tick(10.0f, false, false, 0);
  m.tick(11.75f, false, false, 50);
}

// Stage-1 trigger at 9.75 (travel 2.0 from 11.75): FinalSeek, internal
// back-off sub-stage entered, entry position 9.75@150.
void finalSeekReached(HomingModel& m) {
  seekDownReached(m);
  m.tick(11.0f, false, false, 100);   // seek continues, no trigger
  m.tick(9.75f, true, false, 150);    // stage-1 trigger (the datum face)
}

// Internal back-off consumed at 11.5@200 (travel 1.75): slow down sub-stage
// started from 11.5.
void finalSeekFlipped(HomingModel& m) {
  finalSeekReached(m);
  m.tick(11.5f, false, false, 200);
}

// Ideal full bottom homing: stage-2 trigger at 9.75@250, exactly the
// stage-1 trigger position (datum identity, TC-007).
void bottomHomingDone(HomingModel& m) {
  finalSeekFlipped(m);
  m.tick(9.75f, true, false, 250);
}

}  // namespace

void test_homing_startsIdle() {
  HomingModel m;  // defaults, no configure: must still be sane
  TEST_ASSERT_EQUAL_INT(static_cast<int>(HomePhase::Idle),
                        static_cast<int>(m.phase()));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(HomeFault::None),
                        static_cast<int>(m.fault()));
  TEST_ASSERT_FALSE(m.motionWanted());
  TEST_ASSERT_EQUAL_INT(0, m.direction());
  TEST_ASSERT_FALSE(m.datumSetThisRun());
  TEST_ASSERT_FALSE(m.backOffCappedByTopLimit());
  // ticks outside a run are no-ops
  m.tick(12.0f, true, true, 999999);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(HomePhase::Idle),
                        static_cast<int>(m.phase()));
}

void test_homing_bottomHomingHappyPath() {
  HomingModel m = configured();
  m.start();
  TEST_ASSERT_EQUAL_INT(static_cast<int>(HomePhase::BackOff),
                        static_cast<int>(m.phase()));
  TEST_ASSERT_TRUE(m.motionWanted());
  TEST_ASSERT_EQUAL_INT(+1, m.direction());  // FR-001: back off upward
  TEST_ASSERT_EQUAL_FLOAT(1.0f, m.speedFraction());

  m.tick(10.0f, false, false, 0);
  m.tick(11.75f, false, false, 50);  // up 1.75 = clearance consumed
  TEST_ASSERT_EQUAL_INT(static_cast<int>(HomePhase::SeekDown),
                        static_cast<int>(m.phase()));
  TEST_ASSERT_EQUAL_INT(-1, m.direction());  // seek downward
  TEST_ASSERT_EQUAL_FLOAT(1.0f, m.speedFraction());

  m.tick(11.0f, false, false, 100);  // travelling, switch not reached yet
  TEST_ASSERT_EQUAL_INT(static_cast<int>(HomePhase::SeekDown),
                        static_cast<int>(m.phase()));

  m.tick(9.75f, true, false, 150);  // stage-1 trigger after travel 2.0
  TEST_ASSERT_EQUAL_INT(static_cast<int>(HomePhase::FinalSeek),
                        static_cast<int>(m.phase()));
  TEST_ASSERT_EQUAL_INT(+1, m.direction());  // internal back-off goes up
  TEST_ASSERT_EQUAL_FLOAT(1.0f, m.speedFraction());

  m.tick(11.5f, false, false, 200);  // internal back-off consumed
  TEST_ASSERT_EQUAL_INT(static_cast<int>(HomePhase::FinalSeek),
                        static_cast<int>(m.phase()));
  TEST_ASSERT_EQUAL_INT(-1, m.direction());  // slow re-seek goes down
  TEST_ASSERT_EQUAL_FLOAT(0.25f, m.speedFraction());  // final speed (FR-001)

  // Stage-2 trigger at 9.75: the SAME position as the stage-1 trigger, so
  // the datum is identical between the two touches (TC-007, FR-001).
  m.tick(9.75f, true, false, 250);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(HomePhase::Done),
                        static_cast<int>(m.phase()));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(HomeFault::None),
                        static_cast<int>(m.fault()));
  TEST_ASSERT_TRUE(m.datumSetThisRun());  // datum only from FinalSeek
  TEST_ASSERT_FALSE(m.backOffCappedByTopLimit());
  TEST_ASSERT_FALSE(m.motionWanted());
  TEST_ASSERT_EQUAL_INT(0, m.direction());
  // Done is quiescent: stray switches and clocks change nothing
  m.tick(0.0f, true, true, 999999);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(HomePhase::Done),
                        static_cast<int>(m.phase()));
}

void test_homing_seekDownTriggerAtToleranceBoundaryIsLegal() {
  // FR-001 "before the clearance is consumed": travel == clearance -
  // oneStep is consumed (strict < on the guard), so the trigger is legal.
  // This tolerance is what keeps a legitimately parked carriage out of
  // false InstantTrigger under step-quantised feedback.
  HomingModel m = configured();
  m.start();
  m.tick(10.0f, false, false, 0);
  m.tick(12.0f, false, false, 0);   // back-off done, SeekDown entry at 12.0
  m.tick(10.25f, true, false, 10);  // down 1.75 == clearance - oneStep
  TEST_ASSERT_EQUAL_INT(static_cast<int>(HomePhase::FinalSeek),
                        static_cast<int>(m.phase()));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(HomeFault::None),
                        static_cast<int>(m.fault()));
}

void test_homing_seekDownTriggerBelowToleranceIsInstantTrigger() {
  // One half-step of travel below the tolerance: broken wire per FR-001 /
  // spec A2 (a pressed-at-instant-bottom switch is indistinguishable from
  // a broken wire and must never yield a phantom datum).
  HomingModel m = configured();
  m.start();
  m.tick(10.0f, false, false, 0);
  m.tick(12.0f, false, false, 0);
  m.tick(10.3125f, true, false, 10);  // down 1.6875 < 1.75
  TEST_ASSERT_EQUAL_INT(static_cast<int>(HomePhase::Fault),
                        static_cast<int>(m.phase()));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(HomeFault::InstantTrigger),
                        static_cast<int>(m.fault()));
  TEST_ASSERT_FALSE(m.motionWanted());
  TEST_ASSERT_FALSE(m.datumSetThisRun());
}

void test_homing_brokenWireBottomPressedFromStart() {
  // bottomStable true from the very first tick (broken wire): BackOff
  // ignores it (guard is the clearance, boot state is the selftest's job),
  // and the fault surfaces on the first judged SeekDown tick.
  HomingModel m = configured();
  m.start();
  m.tick(10.0f, true, false, 0);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(HomePhase::BackOff),
                        static_cast<int>(m.phase()));
  m.tick(12.0f, true, false, 50);  // back-off done; bottom still stuck
  TEST_ASSERT_EQUAL_INT(static_cast<int>(HomePhase::SeekDown),
                        static_cast<int>(m.phase()));
  m.tick(11.9f, true, false, 100);  // travel 0.1 -> instant trigger
  TEST_ASSERT_EQUAL_INT(static_cast<int>(HomePhase::Fault),
                        static_cast<int>(m.phase()));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(HomeFault::InstantTrigger),
                        static_cast<int>(m.fault()));
}

void test_homing_finalSeekEarlyReTriggerIsStuckSwitch() {
  // TC-007/FR-001 second stage: the re-seek covers exactly the clearance;
  // a re-trigger before it is consumed is a stuck switch, never a datum.
  HomingModel m = configured();
  finalSeekFlipped(m);
  m.tick(11.4f, true, false, 250);  // down travel 0.1 < 1.75
  TEST_ASSERT_EQUAL_INT(static_cast<int>(HomePhase::Fault),
                        static_cast<int>(m.phase()));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(HomeFault::StuckSwitch),
                        static_cast<int>(m.fault()));
  TEST_ASSERT_FALSE(m.datumSetThisRun());
  TEST_ASSERT_FALSE(m.motionWanted());

  // Same verdict with zero travel on the first down tick (switch never
  // released after the stage-1 touch).
  HomingModel z = configured();
  finalSeekFlipped(z);
  z.tick(11.5f, true, false, 250);  // flip position, travel 0
  TEST_ASSERT_EQUAL_INT(static_cast<int>(HomeFault::StuckSwitch),
                        static_cast<int>(z.fault()));

  // Fault latches (FR-009: recovery by reset/reboot, not by re-homing).
  m.tick(8.75f, true, false, 300);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(HomePhase::Fault),
                        static_cast<int>(m.phase()));
  m.start();
  m.startTopSeek();
  m.startFromTop();
  TEST_ASSERT_EQUAL_INT(static_cast<int>(HomePhase::Fault),
                        static_cast<int>(m.phase()));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(HomeFault::StuckSwitch),
                        static_cast<int>(m.fault()));
}

void test_homing_bounceDuringFinalSeekBackOff() {
  // TC-007 bounce immunity of the second touch: the bottom switch may
  // still read pressed (or bounce) while FinalSeek backs off - only the
  // down sub-stage judges the re-trigger.
  HomingModel m = configured();
  finalSeekReached(m);
  m.tick(10.5f, true, false, 160);  // still pressed mid back-off: ignored
  TEST_ASSERT_EQUAL_INT(static_cast<int>(HomePhase::FinalSeek),
                        static_cast<int>(m.phase()));
  TEST_ASSERT_EQUAL_INT(+1, m.direction());  // still on the up sub-stage
  m.tick(11.5f, false, false, 200);          // back-off consumed, switch freed
  TEST_ASSERT_EQUAL_INT(-1, m.direction());
  m.tick(9.75f, true, false, 250);           // settled re-trigger
  TEST_ASSERT_EQUAL_INT(static_cast<int>(HomePhase::Done),
                        static_cast<int>(m.phase()));
  TEST_ASSERT_TRUE(m.datumSetThisRun());
}

void test_homing_seekDownTimeoutBoundary() {
  // TC-006: timeout on injected elapsed time, strict expiry, per-phase
  // window (entry stamped at 50 here).
  HomingModel m = configured();
  m.start();
  m.tick(10.0f, false, false, 0);
  m.tick(11.75f, false, false, 50);  // SeekDown entry at t=50
  m.tick(11.7f, false, false, 30050);  // delta == timeoutMs: not yet expired
  TEST_ASSERT_EQUAL_INT(static_cast<int>(HomePhase::SeekDown),
                        static_cast<int>(m.phase()));
  m.tick(11.6f, false, false, 30051);  // delta > timeoutMs: expired
  TEST_ASSERT_EQUAL_INT(static_cast<int>(HomePhase::Fault),
                        static_cast<int>(m.phase()));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(HomeFault::SeekTimeout),
                        static_cast<int>(m.fault()));
  TEST_ASSERT_FALSE(m.motionWanted());
  TEST_ASSERT_FALSE(m.datumSetThisRun());
}

void test_homing_finalSeekTimeoutsBothSubStages() {
  // FR-001: the timeout covers the whole FinalSeek phase (plan rule list).
  HomingModel up = configured();
  finalSeekReached(up);  // entry 9.75@150
  up.tick(10.0f, false, false, 30151);  // up sub-stage, delta 30001
  TEST_ASSERT_EQUAL_INT(static_cast<int>(HomeFault::SeekTimeout),
                        static_cast<int>(up.fault()));

  HomingModel down = configured();
  finalSeekFlipped(down);  // entry still stamped at 150 (phase-wide window)
  down.tick(11.4f, false, false, 30151);  // down sub-stage, delta 30001
  TEST_ASSERT_EQUAL_INT(static_cast<int>(HomePhase::Fault),
                        static_cast<int>(down.phase()));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(HomeFault::SeekTimeout),
                        static_cast<int>(down.fault()));
}

void test_homing_seekUpTimeout() {
  // FR-008 + TC-006: the top seek times out like the datum seeks.
  HomingModel m = configured();
  m.startTopSeek();
  m.tick(0.0f, false, false, 0);
  m.tick(1.0f, false, false, 30000);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(HomePhase::SeekUp),
                        static_cast<int>(m.phase()));
  m.tick(2.0f, false, false, 30001);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(HomeFault::SeekTimeout),
                        static_cast<int>(m.fault()));
  TEST_ASSERT_FALSE(m.datumSetThisRun());
}

void test_homing_backOffPhasesHaveNoTimeout() {
  // Plan rule: SeekDown/FinalSeek/SeekUp time out - BackOff and
  // BackOffDown do not (their completion is pure distance).
  HomingModel back = configured();
  back.start();
  back.tick(10.0f, false, false, 0);
  back.tick(10.2f, false, false, 999999);  // huge elapsed, clearance left
  TEST_ASSERT_EQUAL_INT(static_cast<int>(HomePhase::BackOff),
                        static_cast<int>(back.phase()));
  back.tick(11.75f, false, false, 999999);  // completes normally
  TEST_ASSERT_EQUAL_INT(static_cast<int>(HomePhase::SeekDown),
                        static_cast<int>(back.phase()));

  HomingModel down = configured();
  down.startTopSeek();
  down.tick(4.0f, false, false, 0);
  down.tick(4.0f, false, true, 999999);  // top hit, BackOffDown entry
  TEST_ASSERT_EQUAL_INT(static_cast<int>(HomePhase::BackOffDown),
                        static_cast<int>(down.phase()));
  down.tick(2.25f, false, false, 9999999);  // huge elapsed: still completes
  TEST_ASSERT_EQUAL_INT(static_cast<int>(HomePhase::Done),
                        static_cast<int>(down.phase()));
}

void test_homing_topSeekHappyPathNeverSetsDatum() {
  // FR-008 / D12: SeekUp -> BackOffDown, done without a datum; a top hit
  // in the very first tick is legal (no instant-trigger guard on SeekUp).
  HomingModel m = configured();
  m.startTopSeek();
  TEST_ASSERT_EQUAL_INT(static_cast<int>(HomePhase::SeekUp),
                        static_cast<int>(m.phase()));
  TEST_ASSERT_TRUE(m.motionWanted());
  TEST_ASSERT_EQUAL_INT(+1, m.direction());
  TEST_ASSERT_EQUAL_FLOAT(1.0f, m.speedFraction());

  m.tick(0.0f, false, false, 0);
  m.tick(4.0f, false, true, 500);  // D26 triggers
  TEST_ASSERT_EQUAL_INT(static_cast<int>(HomePhase::BackOffDown),
                        static_cast<int>(m.phase()));
  TEST_ASSERT_EQUAL_INT(-1, m.direction());  // back off downward
  TEST_ASSERT_EQUAL_FLOAT(1.0f, m.speedFraction());

  m.tick(2.25f, false, false, 1000);  // down 1.75 = clearance consumed
  TEST_ASSERT_EQUAL_INT(static_cast<int>(HomePhase::Done),
                        static_cast<int>(m.phase()));
  TEST_ASSERT_FALSE(m.datumSetThisRun());  // FR-008: never a datum
  TEST_ASSERT_EQUAL_INT(static_cast<int>(HomeFault::None),
                        static_cast<int>(m.fault()));
}

void test_homing_topSeekImmediateHitIsLegal() {
  // Carriage already at the top: the first SeekUp tick already carries
  // topStable - a legal result, not an instant-trigger fault.
  HomingModel m = configured();
  m.startTopSeek();
  m.tick(9.0f, false, true, 0);  // trigger on the entry tick itself
  TEST_ASSERT_EQUAL_INT(static_cast<int>(HomePhase::BackOffDown),
                        static_cast<int>(m.phase()));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(HomeFault::None),
                        static_cast<int>(m.fault()));
  m.tick(7.25f, false, false, 10);  // back-off down consumed
  TEST_ASSERT_EQUAL_INT(static_cast<int>(HomePhase::Done),
                        static_cast<int>(m.phase()));
  TEST_ASSERT_FALSE(m.datumSetThisRun());
}

void test_homing_topSeekBottomStuckNeverSetsDatum() {
  // Bottom switch pressed/broken through the entire top seek: the bottom
  // input is not consulted on the top path; Done is reached via
  // BackOffDown and still never carries a datum.
  HomingModel m = configured();
  m.startTopSeek();
  m.tick(0.0f, true, false, 0);      // bottom true: ignored by SeekUp
  m.tick(4.0f, true, true, 500);     // top triggers, bottom still true
  TEST_ASSERT_EQUAL_INT(static_cast<int>(HomePhase::BackOffDown),
                        static_cast<int>(m.phase()));
  m.tick(2.25f, true, false, 900);   // bottom true: ignored by BackOffDown
  TEST_ASSERT_EQUAL_INT(static_cast<int>(HomePhase::Done),
                        static_cast<int>(m.phase()));
  TEST_ASSERT_FALSE(m.datumSetThisRun());
  TEST_ASSERT_EQUAL_INT(static_cast<int>(HomeFault::None),
                        static_cast<int>(m.fault()));
}

void test_homing_startFromTopSeeksFromTopAndFlags() {
  // FR-001 note: D26 trips during the initial back-off (runner wires the
  // accepted top flag here); the seek starts immediately from that point.
  HomingModel m = configured();
  m.start();
  m.tick(10.0f, false, false, 0);
  m.tick(10.5f, false, false, 100);  // only 0.5 up when D26 hits
  m.startFromTop();
  TEST_ASSERT_EQUAL_INT(static_cast<int>(HomePhase::SeekDown),
                        static_cast<int>(m.phase()));
  TEST_ASSERT_TRUE(m.backOffCappedByTopLimit());
  TEST_ASSERT_EQUAL_INT(-1, m.direction());

  m.tick(10.5f, false, false, 200);   // SeekDown entry latched here
  m.tick(8.75f, true, false, 300);    // down 1.75 = tolerance boundary
  TEST_ASSERT_EQUAL_INT(static_cast<int>(HomePhase::FinalSeek),
                        static_cast<int>(m.phase()));
  m.tick(10.5f, false, false, 400);   // internal back-off consumed
  m.tick(8.75f, true, false, 500);    // settled re-trigger -> datum
  TEST_ASSERT_EQUAL_INT(static_cast<int>(HomePhase::Done),
                        static_cast<int>(m.phase()));
  TEST_ASSERT_TRUE(m.datumSetThisRun());
  TEST_ASSERT_TRUE(m.backOffCappedByTopLimit());  // survives for the report

  m.start();  // next run clears the cap flag
  TEST_ASSERT_FALSE(m.backOffCappedByTopLimit());
}

void test_homing_rehomeFromDoneWorksIdempotently() {
  // FR-001 idempotence: running homing twice is legal; each run re-sets
  // its datum at its own stage-2 trigger position.
  HomingModel m = configured();
  bottomHomingDone(m);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(HomePhase::Done),
                        static_cast<int>(m.phase()));

  m.start();  // legal from Done
  TEST_ASSERT_EQUAL_INT(static_cast<int>(HomePhase::BackOff),
                        static_cast<int>(m.phase()));
  m.tick(10.0f, false, false, 600);
  m.tick(11.75f, false, false, 650);
  m.tick(9.5f, true, false, 750);    // stage-1 trigger at 9.5
  TEST_ASSERT_EQUAL_INT(static_cast<int>(HomePhase::FinalSeek),
                        static_cast<int>(m.phase()));
  m.tick(11.25f, false, false, 800);
  m.tick(9.5f, true, false, 850);    // stage-2 at the same 9.5 -> datum
  TEST_ASSERT_EQUAL_INT(static_cast<int>(HomePhase::Done),
                        static_cast<int>(m.phase()));
  TEST_ASSERT_TRUE(m.datumSetThisRun());
}

void test_homing_guardedCallsAreNoops() {
  HomingModel m = configured();
  m.startFromTop();  // Idle: ignored
  TEST_ASSERT_EQUAL_INT(static_cast<int>(HomePhase::Idle),
                        static_cast<int>(m.phase()));
  TEST_ASSERT_FALSE(m.backOffCappedByTopLimit());

  m.start();
  m.tick(10.0f, false, false, 0);
  m.start();  // BackOff: ignored, no silent restart
  TEST_ASSERT_EQUAL_INT(static_cast<int>(HomePhase::BackOff),
                        static_cast<int>(m.phase()));
  m.startTopSeek();  // active run: ignored
  TEST_ASSERT_EQUAL_INT(static_cast<int>(HomePhase::BackOff),
                        static_cast<int>(m.phase()));

  m.tick(11.75f, false, false, 50);
  m.startTopSeek();  // SeekDown: ignored
  TEST_ASSERT_EQUAL_INT(static_cast<int>(HomePhase::SeekDown),
                        static_cast<int>(m.phase()));

  m.startFromTop();  // SeekDown (not BackOff): ignored
  TEST_ASSERT_EQUAL_INT(static_cast<int>(HomePhase::SeekDown),
                        static_cast<int>(m.phase()));
  TEST_ASSERT_FALSE(m.backOffCappedByTopLimit());

  m.tick(9.75f, true, false, 150);
  m.tick(11.5f, false, false, 200);
  m.tick(9.75f, true, false, 250);  // Done
  m.startFromTop();                 // Done: ignored
  TEST_ASSERT_EQUAL_INT(static_cast<int>(HomePhase::Done),
                        static_cast<int>(m.phase()));
}

void test_homing_configureResetsStateAndAppliesParams() {
  HomingModel m = configured();
  m.start();
  m.tick(10.0f, false, false, 0);
  m.tick(12.0f, false, false, 0);
  m.tick(11.9f, true, false, 10);  // InstantTrigger
  TEST_ASSERT_EQUAL_INT(static_cast<int>(HomePhase::Fault),
                        static_cast<int>(m.phase()));

  m.configure(4.0f, 0.5f, 5000, 0.5f);  // new geometry, threshold 3.5
  TEST_ASSERT_EQUAL_INT(static_cast<int>(HomePhase::Idle),
                        static_cast<int>(m.phase()));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(HomeFault::None),
                        static_cast<int>(m.fault()));
  TEST_ASSERT_FALSE(m.datumSetThisRun());

  m.start();
  m.tick(10.0f, false, false, 0);
  m.tick(13.25f, false, false, 10);  // up 3.25 < 3.5: old clearance is dead
  TEST_ASSERT_EQUAL_INT(static_cast<int>(HomePhase::BackOff),
                        static_cast<int>(m.phase()));
  m.tick(13.5f, false, false, 20);   // up 3.5 consumed with new tolerance
  TEST_ASSERT_EQUAL_INT(static_cast<int>(HomePhase::SeekDown),
                        static_cast<int>(m.phase()));
}

void test_homing_ignoresNonFinitePosition() {
  // NaN/inf position feedback poisons every travel comparison; the whole
  // tick must be dropped (mirrors the BreakDetector NaN policy).
  HomingModel m = configured();
  seekDownReached(m);
  m.tick(std::nanf(""), true, false, 60);   // would be InstantTrigger if used
  TEST_ASSERT_EQUAL_INT(static_cast<int>(HomePhase::SeekDown),
                        static_cast<int>(m.phase()));
  m.tick(INFINITY, true, false, 70);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(HomePhase::SeekDown),
                        static_cast<int>(m.phase()));
  m.tick(9.75f, true, false, 150);  // valid tick proceeds normally
  TEST_ASSERT_EQUAL_INT(static_cast<int>(HomePhase::FinalSeek),
                        static_cast<int>(m.phase()));
}

void runHomingModelTests() {
  RUN_TEST(test_homing_startsIdle);
  RUN_TEST(test_homing_bottomHomingHappyPath);
  RUN_TEST(test_homing_seekDownTriggerAtToleranceBoundaryIsLegal);
  RUN_TEST(test_homing_seekDownTriggerBelowToleranceIsInstantTrigger);
  RUN_TEST(test_homing_brokenWireBottomPressedFromStart);
  RUN_TEST(test_homing_finalSeekEarlyReTriggerIsStuckSwitch);
  RUN_TEST(test_homing_bounceDuringFinalSeekBackOff);
  RUN_TEST(test_homing_seekDownTimeoutBoundary);
  RUN_TEST(test_homing_finalSeekTimeoutsBothSubStages);
  RUN_TEST(test_homing_seekUpTimeout);
  RUN_TEST(test_homing_backOffPhasesHaveNoTimeout);
  RUN_TEST(test_homing_topSeekHappyPathNeverSetsDatum);
  RUN_TEST(test_homing_topSeekImmediateHitIsLegal);
  RUN_TEST(test_homing_topSeekBottomStuckNeverSetsDatum);
  RUN_TEST(test_homing_startFromTopSeeksFromTopAndFlags);
  RUN_TEST(test_homing_rehomeFromDoneWorksIdempotently);
  RUN_TEST(test_homing_guardedCallsAreNoops);
  RUN_TEST(test_homing_configureResetsStateAndAppliesParams);
  RUN_TEST(test_homing_ignoresNonFinitePosition);
}
