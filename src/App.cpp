#include "App.h"

#include "ManualMoveGuard.h"
#include "Text.h"

void App::begin() {
  Serial.begin(115200);

  motion_.begin();

  // M2 FR-002: the stored locale (NVS, default EN) decides all CLI text
  // before the first line is printed.
  setTextLocale(settings_.loadLocale());

  delay(500);
  Serial.println();
  Serial.println(msg(Msg::BootBanner));
  Serial.println(msg(Msg::MotorsSyncNote));

  loadcell_.begin(settings_);
  powerSense_.begin();
  endstops_.begin();  // M3-T008: endstop inputs (levels live from boot)
  // M3-T012: test config from NVS (defaults on first boot; clamped on
  // load - the single validation boundary is clampTestConfig).
  testRunner_.applyConfig(settings_.loadTestConfig());

  // M1: blocking startup selftest (FR-007 boot log); must finish before
  // the Web UI answers so /api/status can already report the checks.
  // M3-T013: endstop boot levels join as checks 6+7.
  diagnostics_.runBootSelfTest(settings_, loadcell_, motion_, powerSense_,
                               endstops_);

  // M8 FR-009: LED configured only after the boot decisions (GPIO2 is a
  // strapping pin); the pattern engine starts with the post-selftest
  // state, so Idle shows the heartbeat and a boot FAIL shows Error.
  led_.begin(millis());

  webUi_.begin();
}

void App::reportRunnerEvents() {
  const TestState testState = testRunner_.testState();
  const HomePhase homePhase = testRunner_.homing().phase();
  const HoldController::Phase holdPhase = testRunner_.holdPhase();
  const bool first = !started_;
  started_ = true;

  // Standalone homing results (an in-cycle homing is reported by the
  // cycle summary below - its Done leads straight into Pulling).
  // M6 #192: runner outcomes are JSON event lines - {"event":"runner",
  // "data":{"kind":...},"state":{...}} - one write each, state snapshot
  // taken at emission time.
  if (!first && homePhase == HomePhase::Done &&
      lastHomePhase_ != HomePhase::Done &&
      testState != TestState::Homing && lastTestState_ != TestState::Homing) {
    char line[256];
    ApiRunnerData d{};
    if (testRunner_.homing().datumSetThisRun()) {
      // This completed run established the datum (also true for an
      // idempotent re-home); `home top` never sets it (D12).
      d.kind = ApiRunnerEvent::HomingDatum;
    } else if (testRunner_.topSeekSpanValid()) {
      d.kind = ApiRunnerEvent::HomeTop;
      d.spanMm = testRunner_.topSeekSpanMm();
    } else {
      d.kind = ApiRunnerEvent::HomeTopNoDatum;
    }
    formatApiRunnerEvent(d, commands_.stateSnapshot(), line, sizeof(line));
    Serial.println(line);
  }

  if (!first && testState != lastTestState_) {    char line[256];
    switch (testState) {
      case TestState::Tail:
        // M8-T003: the break overlay (emission window: Running, FR-005).
        // Buffer-full also enters Tail - only a real break latches the
        // LED event.
        if (testRunner_.breakDetected()) {
          pendingBreakLed_ = true;
        }
        break;
      case TestState::Done:
        // FR-002(6): peak force, break displacement, sample count.
        {
          ApiRunnerData d{};
          d.kind = ApiRunnerEvent::TestSummary;
          d.peakN = testRunner_.peakForceN();
          d.breakMm = testRunner_.breakDisplacementMm();
          d.samples = static_cast<unsigned>(
              testRunner_.samples().count());
          d.truncated = testRunner_.samples().truncated();
          formatApiRunnerEvent(d, commands_.stateSnapshot(), line,
                               sizeof(line));
          Serial.println(line);
        }
        break;
      case TestState::Limit: {
        ApiRunnerData d{};
        d.kind = ApiRunnerEvent::TestLimit;
        formatApiRunnerEvent(d, commands_.stateSnapshot(), line,
                             sizeof(line));
        Serial.println(line);
        break;
      }
      case TestState::Aborted: {
        ApiRunnerData d{};
        d.kind = ApiRunnerEvent::TestAborted;
        formatApiRunnerEvent(d, commands_.stateSnapshot(), line,
                             sizeof(line));
        Serial.println(line);
        break;
      }
      default:
        break;  // Error: the STATE line (Diagnostics) already reports it
    }
  }
  // M4 #210: hold outcomes are hold_summary events - emitted on the
  // transition INTO a terminal hold phase (Done | Limit | Lost |
  // Aborted), the same single-edge rule the other runner events use.
  if (!first && holdPhase != lastHoldPhase_ &&
      (holdPhase == HoldController::Phase::Done ||
       holdPhase == HoldController::Phase::Limit ||
       holdPhase == HoldController::Phase::Lost ||
       holdPhase == HoldController::Phase::Aborted) &&
      testRunner_.holdReportValid()) {
    char line[256];
    ApiRunnerData d{};
    d.kind = ApiRunnerEvent::HoldSummary;
    switch (testRunner_.holdEndReason()) {
      case HoldController::End::Time: d.holdEnd = "time"; break;
      case HoldController::End::Stop: d.holdEnd = "stop"; break;
      case HoldController::End::Limit: d.holdEnd = "limit"; break;
      case HoldController::End::Lost: d.holdEnd = "lost"; break;
      case HoldController::End::Fault: d.holdEnd = "fault"; break;
      case HoldController::End::None: d.holdEnd = "none"; break;
    }
    const HoldController::Report &r = testRunner_.holdReport();
    d.holdTicks = r.holdTicks;
    d.meanN = r.meanForceN;
    d.maxDevPct = r.maxDevPct;
    d.inBandPct = r.inBandPct;
    d.creepMm = r.creepMm;
    formatApiRunnerEvent(d, commands_.stateSnapshot(), line, sizeof(line));
    Serial.println(line);
  }
  lastTestState_ = testState;
  lastHomePhase_ = homePhase;
  lastHoldPhase_ = holdPhase;
}

void App::loop() {
  webUi_.handleClient();
  loadcell_.poll();
  endstops_.poll();  // M3-T008: 5 ms debounce cadence (edges for M8-T002)
  serialCli_.poll();

  // FR-005: 1 Hz runtime power monitoring; a debounced FAIL drives the
  // model to Error (STATE line), FR-009 recovery by reset/reboot.
  powerSense_.poll();
  diagnostics_.reportRuntimePowerState(powerSense_.state());

  // M3-T011: runner tick AFTER the loadcell/endstop polling (fresh 10 Hz
  // sample + debounced levels) and the power evaluation, so a power FAIL
  // is visible on the same tick. A runner fault (homing 0x05xx) lands in
  // the model BEFORE the Error-transition block below, disabling the
  // driver in the same loop iteration.
  testRunner_.tick(millis(),
                   diagnostics_.model().state() == DeviceState::Error);
  reportRunnerEvents();
  commands_.pollDump();  // M3-T012: CSV dump pump (D8 chunking)

  // #158 (M3 FR-009): plain (non-runner) moves respect the endstops -
  // level-triggered forceStop in the direction of travel. Checked AFTER
  // the runner tick so an active homing/cycle keeps its own endstop
  // handling (a homing back-off must be able to LEAVE a pressed switch);
  // the guard never fires for runner-driven motion.
  if (!testRunner_.active() && motion_.isRunning() &&
      manualMoveBlockedByEndstop(motion_.moveDirection(),
                                 endstops_.bottom(), endstops_.top())) {
    motion_.forceStop();
  }

  // FR-009: on the transition into Error the driver must be off
  // (boot faults already disable it in Diagnostics; this covers the
  // runtime power path).
  const DeviceState modelState = diagnostics_.model().state();
  if (modelState == DeviceState::Error &&
      lastModelState_ != DeviceState::Error) {
    motion_.disable();
  }
  lastModelState_ = modelState;

  // M8-T003: cycle refinement under a healthy device (FR-007: the LED
  // module owns no state - App maps DeviceModel + cycle state). A test
  // pull (and its tail) reads as Running; a homing (standalone or
  // in-cycle) as Homing.
  led::LedBase ledBase = ledBaseFor(modelState);
  if (ledBase == led::LedBase::Idle) {
    const TestState testState = testRunner_.testState();
    if (testState == TestState::Pulling || testState == TestState::Tail) {
      ledBase = led::LedBase::Running;
    } else if (testState == TestState::Homing ||
               testRunner_.homing().motionWanted()) {
      ledBase = led::LedBase::Homing;
    }
  }

  // M8: endstop edges -> LED overlay (FR-005 emission window: forwarded
  // from every non-Error state; Error keeps the LED per FR-008). Edges
  // are read-and-clear, so they never queue across states. Break and
  // CalApplied are latest-wins per the FR-005 overlay rule.
  led::LedEvent ledEvent = led::LedEvent::None;
  if (modelState != DeviceState::Error) {
    if (pendingBreakLed_) {
      pendingBreakLed_ = false;
      ledEvent = led::LedEvent::Break;
    } else if (commands_.consumeCalAppliedEvent()) {
      // Window: calibration fires only in Idle (FR-005); the flag was
      // set on cal_apply success in Commands.
      if (modelState == DeviceState::Idle) {
        ledEvent = led::LedEvent::CalApplied;
      }
    } else if (endstops_.bottomEdge() || endstops_.topEdge()) {
      ledEvent = led::LedEvent::EndstopEdge;
    }
  } else {
    pendingBreakLed_ = false;
    commands_.consumeCalAppliedEvent();  // FR-008: no overlay in Error
  }
  led_.tick(ledBase, ledEvent, millis());

  const unsigned long now = millis();
  if (now - lastStatusMs_ >= kStatusIntervalMs) {
    lastStatusMs_ = now;
    // D14 (M3): while a CSV dump is in flight the serial line belongs to
    // it alone - an interleaved status line would break the host parser.
    // M6-T006 (plan D3): the periodic status is one JSON event line;
    // parsers distinguish it from replies by the leading "event" key.
    if (!testRunner_.dumpInProgress() &&
        (motion_.isEnabled() || loadcell_.isReady())) {
      char line[256];
      formatApiEvent("status", commands_.stateSnapshot(), line, sizeof(line));
      Serial.println(line);
    }
  }
}
