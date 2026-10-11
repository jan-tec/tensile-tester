// Test cycle runner (M3 T011): the hardware glue over the three
// host-tested models (HomingModel, TestCycleModel, BreakDetector - plan
// section 6). Owns the SampleBuffer (64 KB static, D7) and the run
// record; ticks from App::loop() AFTER loadcell_.poll() (10 Hz samples
// are fresh) and the runtime power evaluation (a power FAIL is visible
// on the same tick).
//
// Division of labour (D2): all decisions live in the pure models; this
// class only actuates (IMotion startContinuous/forceStop/zero, endstop
// levels, raw force) and translates HomeFault -> stable 0x05xx codes
// into DeviceModel Error via Diagnostics (the model stays code-free,
// plan section 5). No host unit test by design (host/target boundary);
// verified by the T014 bench matrix (blocked on #76 wiring).
//
// Cycle semantics (spec FR-002):
//   test -> (home if no datum this power cycle, #81) -> pull up at the
//   test speed -> 10 Hz {displacement, raw force} samples + break
//   verdict -> forceStop on break -> 1 s sampling-only tail (FR-002(5))
//   -> Done. D26 during pull = hard backstop -> Limit, NO code (D10).
//   Buffer full -> controlled stop -> Tail with `truncated` (FR-004).
//
// Tail note: TestCycle.h's state gloss says "post-break downward travel"
// but spec FR-002(5) ("keep sampling a fixed tail window") and plan
// section 2 ("Tail (1 s, still sampling)") specify sampling-only; this
// implements sampling-only (stationary carriage after forceStop). Flag
// for the T014 B1 bench check.
#pragma once

#include <Arduino.h>

#include <BreakDetect.h>
#include <HoldController.h>
#include <Homing.h>
#include <TestConfig.h>
#include <TestCycle.h>
#include <TestData.h>

#include "Diagnostics.h"
#include "Endstops.h"
#include "ILoadCell.h"
#include "IMotion.h"

class TestRunner {
 public:
  // FR-002(5): fixed post-break tail window.
  static constexpr unsigned long kTailMs = 1000;
  // FR-004: HX711 native cadence (mirrors LoadCell::kSampleIntervalMs).
  static constexpr unsigned long kSampleIntervalMs = 100;
  // FR-001: slow re-seek at 25 % of the seek speed (spec default; not a
  // TestConfig knob).
  static constexpr float kFinalSeekSpeedFraction = 0.25f;

  TestRunner(IMotion &motion, ILoadCell &loadcell, Endstops &endstops,
             Diagnostics &diagnostics)
      : motion_(motion),
        loadcell_(loadcell),
        endstops_(endstops),
        diagnostics_(diagnostics) {
    applyConfig(defaultTestConfig());
  }

  // Config (FR-005): the single validation boundary is clampTestConfig;
  // re-parameterises both models. NOTE: HomingModel::configure() also
  // resets the homing FSM - call while idle (T012 owns the NVS load and
  // the testspeed/testarm/testdrop updates).
  void applyConfig(const TestConfig &config);
  const TestConfig &config() const { return config_; }

  // `home` (bottom datum, FR-001) / `home top` (FR-008, never a datum).
  // False while anything runs (D11) or the homing FSM is latched in
  // Fault (recovery is reset/reboot, M1 FR-009).
  bool startHome(bool topOnly);

  // `test` (FR-002). False while active or the cycle is in Error (its
  // only exit is Reset = reboot). Keeps an existing datum (#81): the
  // operator may have inserted the sample after homing.
  bool startTest();

  // `chold <N> [min]` (M4 FR-001): constant-force hold. Same gates as
  // startTest (exclusivity, latched homing fault); setpoint clamps via
  // clampHoldSetpointN and 0/non-finite is rejected (caller maps to an
  // error reply). Duration minutes -> ms; 0 = until `stop`. Homing
  // reuses the M3 FSM (FR-007) - standalone if no datum, skipped if
  // homed (#81 analogue).
  bool startHold(float setpointN, float durationMin);

  // The last hold's outcome (T006 renders the summary/event; valid
  // after a terminal phase, survives later config writes).
  bool holdReportValid() const { return holdReportValid_; }
  HoldController::End holdEndReason() const { return holdEnd_; }
  const HoldController::Report &holdReport() const { return holdReport_; }

  // `stop` routes here first (T012): aborts a running cycle (Aborted)
  // or a standalone homing; leaves the data buffer untouched. False
  // when nothing was running.
  bool abort();

  // One step from App::loop(). deviceInError: the DeviceModel is in
  // Error (e.g. runtime power FAIL, M1 FR-005) -> quiesce; the cycle
  // records Error with the data retained (US3, A3).
  void tick(unsigned long nowMs, bool deviceInError);

  // D11 exclusivity: a homing (standalone or in-cycle), a cycle run, or
  // a hold (M4 FR-001).
  bool active() const {
    return cycle_.active() || homing_.motionWanted() || hold_.running();
  }

  // FR-006: bottom datum valid for this power cycle only (A2
  // broken-wire rule: a datum is only valid while the endstop circuit
  // that produced it is powered - deliberately not NVS-persisted).
  bool homed() const { return homed_; }
  // Bench telemetry (T014): debounced endstop levels for the `ends`
  // debug command - the one state bit the homing diagnosis needed.
  bool endstopBottom() const { return endstops_.bottom(); }
  bool endstopTop() const { return endstops_.top(); }
  TestState testState() const { return cycle_.state(); }
  const HomingModel &homing() const { return homing_; }
  // M4: live hold phase for status surfaces and the runner-event edge.
  HoldController::Phase holdPhase() const { return hold_.phase(); }

  // Run record (summary / testdata replies, T012). Peak force is the
  // detector's armed-phase peak; displacement is relative to the pull
  // start (spec US2 "displacement mm"). The peak lives in the RUN RECORD
  // (captured at finishRun), not in the detector: applyConfig() may run
  // between the run and the report (e.g. a no-op `testspeed <same>`), and
  // configure() resets the detector - break fields already survived this
  // way; a detector-held peak did not (found on the bench 2026-10-11:
  // testdata peak=0 after a no-op config write following a 127.87 N run).
  float peakForceN() const { return runPeakForceN_; }
  uint32_t peakIndex() const { return runPeakIndex_; }
  bool breakDetected() const { return breakDetected_; }
  float breakDisplacementMm() const { return breakPositionMm_; }

  // `home top` report (FR-008): the reached span, valid only when the
  // bottom datum is known.
  bool topSeekSpanValid() const { return topSeekSpanValid_; }
  float topSeekSpanMm() const { return topSeekSpanMm_; }

  // CSV dump plumbing (D8/D14; the chunked writer is Commands, T012).
  // nextDumpIndex resets to 0 per run (buffer cleared in startTest).
  const SampleBuffer &samples() const { return buffer_; }
  size_t nextDumpIndex() const { return nextDumpIndex_; }
  // Dump progress update that KEEPS the dump in flight (D14 suppression
  // stays active between chunks); endDump() finishes it.
  void advanceDump(size_t nextIndex) { nextDumpIndex_ = nextIndex; }
  void beginDump() { dumpInProgress_ = true; }  // from nextDumpIndex_
  void beginDump(size_t fromIndex) {  // resume: `data <from-index>`
    nextDumpIndex_ = fromIndex;
    dumpInProgress_ = true;
  }
  void endDump(size_t nextIndex) {
    dumpInProgress_ = false;
    nextDumpIndex_ = nextIndex;
  }
  bool dumpInProgress() const { return dumpInProgress_; }

 private:
  void beginPull(unsigned long nowMs);
  void tickPull(unsigned long nowMs);
  void tickTail(unsigned long nowMs);
  void tickHold(unsigned long nowMs);   // M4: 10 Hz controller drive
  void applyHoldOutput(const HoldController::Output &out);
  void driveHoldSeek(int seekDir);      // approach/transit continuous seek
  void finishHold(const HoldController::Output &out);
  void quiesceOnDeviceError();
  void handleHomingTransition();
  void driveHomingMotion();
  void reportHomingFault();
  void finishRun();  // restore the operator's rpm; clear motion tracking
  float seekRpm() const;

  IMotion &motion_;
  ILoadCell &loadcell_;
  Endstops &endstops_;
  Diagnostics &diagnostics_;

  TestConfig config_;
  TestCycleModel cycle_;
  HomingModel homing_;
  BreakDetector detector_;
  HoldController hold_;  // M4: the pure hold brain (runner actuates)
  SampleBuffer buffer_;  // 64 KB; TestRunner lives in static storage

  bool homed_ = false;
  bool holdHomingDone_ = false;   // M4: datum ready signal into hold_
  float holdOriginMm_ = 0.0f;     // M4: CSV displacement origin (approach start)
  bool holdOriginSet_ = false;
  bool holdReportValid_ = false;
  HoldController::End holdEnd_ = HoldController::End::None;
  HoldController::Report holdReport_{};
  bool breakDetected_ = false;
  float breakPositionMm_ = 0.0f;
  float runPeakForceN_ = 0.0f;  // run record; see peakForceN() note
  uint32_t runPeakIndex_ = 0;
  float pullStartMm_ = 0.0f;
  unsigned long lastSampleMs_ = 0;
  unsigned long tailStartMs_ = 0;
  bool topSeekSpanValid_ = false;
  float topSeekSpanMm_ = 0.0f;

  // Motion command tracking: re-issue setOutputRpm/startContinuous only
  // on a direction or speed change (FastAccelStepper re-issues are safe
  // but unnecessary; phase changes must re-command).
  float savedRpm_ = 0.0f;  // operator's rpm, restored when a run ends
  int commandedDir_ = 0;
  float commandedRpm_ = 0.0f;

  size_t nextDumpIndex_ = 0;
  bool dumpInProgress_ = false;
};
