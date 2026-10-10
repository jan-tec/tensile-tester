// Test cycle runner implementation (M3 T011). See TestRunner.h for the
// architecture note; hardware timing is target-tested (T014 matrix).
#include "TestRunner.h"

#include <FirmwareCore.h>

void TestRunner::applyConfig(const TestConfig &config) {
  config_ = clampTestConfig(config);
  homing_.configure(config_.homingClearanceMm, kFinalSeekSpeedFraction,
                    config_.homingTimeoutMs,
                    SPINDLE_PITCH_MM_PER_REV / OUTPUT_STEPS_PER_REV);
  detector_.configure(config_.armForceN, config_.dropPercent / 100.0f);
}

bool TestRunner::startHome(bool topOnly) {
  if (active()) {
    return false;
  }
  if (topOnly) {
    homing_.startTopSeek();
  } else {
    homing_.start();
  }
  if (!homing_.motionWanted()) {
    // FSM not startable (latched Fault); recovery is reset/reboot.
    return false;
  }
  savedRpm_ = motion_.outputRpm();
  topSeekSpanValid_ = false;
  return true;
}

bool TestRunner::startTest() {
  if (active() || cycle_.state() == TestState::Error) {
    return false;
  }
  if (homing_.phase() == HomePhase::Fault) {
    return false;  // latched homing fault; the model is in Error anyway
  }
  buffer_.clear();
  nextDumpIndex_ = 0;
  dumpInProgress_ = false;
  breakDetected_ = false;
  breakPositionMm_ = 0.0f;
  savedRpm_ = motion_.outputRpm();
  cycle_.dispatch(TestEvent::Start);  // -> Homing (or restart from an exit)
  if (homed_) {
    // #81: keep the existing datum - the operator may have inserted the
    // sample after homing. FR-002(2-3): straight to the pull.
    beginPull(millis());
  } else {
    // FR-002(1): in-cycle homing; its completion dispatches HomeDone.
    homing_.start();
  }
  return true;
}

bool TestRunner::abort() {
  if (cycle_.active()) {
    cycle_.dispatch(TestEvent::StopRequested);  // -> Aborted
    // An in-cycle homing FSM must die with the cycle, or it would keep
    // driving the motor after the abort (reset is safe from any phase:
    // homed_ is tracked separately).
    homing_.reset();
    motion_.forceStop();
    commandedDir_ = 0;
    finishRun();
    return true;
  }
  if (homing_.motionWanted()) {
    homing_.reset();  // standalone homing has no Aborted state; back to Idle
    motion_.forceStop();
    commandedDir_ = 0;
    finishRun();
    return true;
  }
  return false;
}

void TestRunner::tick(unsigned long nowMs, bool deviceInError) {
  if (deviceInError) {
    quiesceOnDeviceError();
    return;
  }

  const HomePhase before = homing_.phase();

  // FR-001 note: D26 hit during the back-off - the seek starts
  // immediately from the current position (startFromTop() is only legal
  // in BackOff; the report flag comes from the model).
  if (before == HomePhase::BackOff && endstops_.top()) {
    motion_.forceStop();
    commandedDir_ = 0;
    homing_.startFromTop();
  }

  homing_.tick(motion_.positionMm(), endstops_.bottom(), endstops_.top(),
               static_cast<uint32_t>(nowMs));
  if (homing_.phase() != before) {
    handleHomingTransition();
  }
  if (homing_.motionWanted()) {
    driveHomingMotion();
  }

  switch (cycle_.state()) {
    case TestState::Pulling:
      tickPull(nowMs);
      break;
    case TestState::Tail:
      tickTail(nowMs);
      break;
    default:
      break;
  }
}

void TestRunner::handleHomingTransition() {
  switch (homing_.phase()) {
    case HomePhase::FinalSeek:
      // Stage-1 trigger: stop the downward seek instantly (the switch
      // face is a hard stop); the model's internal back-off takes over.
      motion_.forceStop();
      commandedDir_ = 0;
      break;

    case HomePhase::BackOffDown:
      // `home top` trigger (FR-008): hard stop at D26, record the span
      // (only meaningful with a known datum), then back down.
      motion_.forceStop();
      commandedDir_ = 0;
      if (homed_) {
        topSeekSpanValid_ = true;
        topSeekSpanMm_ = motion_.positionMm();
      }
      break;

    case HomePhase::Done:
      motion_.forceStop();
      commandedDir_ = 0;
      if (homing_.datumSetThisRun()) {
        // FR-006 / plan section 4: the datum is simply zero at the
        // stage-2 trigger - no offset arithmetic.
        motion_.zero();
        homed_ = true;
      }
      finishRun();
      if (cycle_.state() == TestState::Homing) {
        beginPull(millis());
      }
      break;

    case HomePhase::Fault:
      motion_.forceStop();
      commandedDir_ = 0;
      finishRun();
      reportHomingFault();
      if (cycle_.state() == TestState::Homing) {
        cycle_.dispatch(TestEvent::HomeFault);  // -> cycle Error
      }
      break;

    default:
      break;  // BackOff -> SeekDown: only a direction change
  }
}

void TestRunner::driveHomingMotion() {
  const int dir = homing_.direction();
  if (dir == 0) {
    return;
  }
  const float rpm = seekRpm() * homing_.speedFraction();
  if (dir == commandedDir_ && rpm == commandedRpm_) {
    return;
  }
  motion_.setOutputRpm(rpm);
  motion_.startContinuous(dir);
  commandedDir_ = dir;
  commandedRpm_ = rpm;
}

void TestRunner::beginPull(unsigned long nowMs) {
  cycle_.dispatch(TestEvent::HomeDone);  // Homing -> Pulling
  detector_.reset();
  pullStartMm_ = motion_.positionMm();
  breakDetected_ = false;
  breakPositionMm_ = 0.0f;
  // CSV index 0 is the pull start (displacement 0, FR-004): force the
  // first sample on this very tick.
  lastSampleMs_ = nowMs - kSampleIntervalMs;
  tailStartMs_ = nowMs;
  const float rpm = feedMmPerMinToRpm(config_.testSpeedMmPerMin);
  motion_.setOutputRpm(rpm);
  motion_.startContinuous(+1);
  commandedDir_ = +1;
  commandedRpm_ = rpm;
}

void TestRunner::tickPull(unsigned long nowMs) {
  // FR-002: D26 during pull = hard backstop; a guarded stop (Limit),
  // never a fault, no machine code (D10).
  if (endstops_.top()) {
    motion_.forceStop();
    commandedDir_ = 0;
    cycle_.dispatch(TestEvent::TopLimitHit);
    finishRun();
    return;
  }
  if (nowMs - lastSampleMs_ < kSampleIntervalMs) {
    return;
  }
  lastSampleMs_ = nowMs;

  const float forceN = loadcell_.rawForceNewton();
  const TestSample sample{motion_.positionMm() - pullStartMm_, forceN};
  if (!buffer_.push(sample)) {
    // FR-004: buffer full - pull stops safely (decel ramp is fine here,
    // there is no mechanical event), truncation is flagged by the buffer.
    cycle_.dispatch(TestEvent::BufferFull);
    motion_.stop();
    commandedDir_ = 0;
    tailStartMs_ = nowMs;
    return;
  }
  // FR-003: the detector sees the SAME raw sample that was recorded -
  // the break sample (dropped force) stays in the curve.
  if (detector_.sample(forceN) == BreakDetector::Verdict::Broken) {
    cycle_.dispatch(TestEvent::BreakDetected);
    // FR-002(4): forceStop semantics (instant, no ramp) - the TC-003
    // over-travel bound depends on it.
    motion_.forceStop();
    commandedDir_ = 0;
    breakDetected_ = true;
    breakPositionMm_ = sample.positionMm;
    tailStartMs_ = nowMs;
  }
}

void TestRunner::tickTail(unsigned long nowMs) {
  // FR-002(5) / FR-004: sampling continues through the fixed tail
  // window (carriage stationary after the break stop). A full buffer
  // keeps its truncated flag; push results are irrelevant here.
  if (nowMs - lastSampleMs_ >= kSampleIntervalMs) {
    lastSampleMs_ = nowMs;
    buffer_.push({motion_.positionMm() - pullStartMm_,
                  loadcell_.rawForceNewton()});
  }
  if (nowMs - tailStartMs_ >= kTailMs) {
    cycle_.dispatch(TestEvent::TailComplete);  // -> Done
    finishRun();
  }
}

void TestRunner::quiesceOnDeviceError() {
  // US3: the DeviceModel entered Error (e.g. runtime power FAIL; the
  // driver is disabled by App's Error-transition block). The cycle
  // records Error and the data stays retrievable (A3).
  if (cycle_.active()) {
    cycle_.dispatch(TestEvent::PowerFail);  // -> Error
    homing_.reset();  // an in-cycle homing dies with the cycle
    motion_.forceStop();
    commandedDir_ = 0;
    finishRun();
  } else if (homing_.motionWanted()) {
    homing_.reset();
    motion_.forceStop();
    commandedDir_ = 0;
    finishRun();
  }
}

void TestRunner::reportHomingFault() {
  // Plan section 5 fault -> code mapping; the DeviceModel effect (Error,
  // driver off) is the normal M1 FR-009 path via Diagnostics.
  uint16_t code = 0;
  switch (homing_.fault()) {
    case HomeFault::SeekTimeout:
      code = static_cast<uint16_t>(ErrorCode::EndstopSeekTimeout);
      break;
    case HomeFault::InstantTrigger:
      code = static_cast<uint16_t>(ErrorCode::EndstopInstantTrigger);
      break;
    case HomeFault::StuckSwitch:
      code = static_cast<uint16_t>(ErrorCode::EndstopStuckSwitch);
      break;
    case HomeFault::None:
      return;
  }
  diagnostics_.reportRuntimeFault(code);
}

void TestRunner::finishRun() {
  // Restore the operator's speed setting (jog/move `rpm`); the driver
  // stays enabled - a finished run is not a fault.
  motion_.setOutputRpm(savedRpm_);
  commandedDir_ = 0;
  commandedRpm_ = savedRpm_;
}

float TestRunner::seekRpm() const {
  // FR-005: seek == 0 means "follow the test speed".
  return feedMmPerMinToRpm(homingSeekSpeed(config_));
}
