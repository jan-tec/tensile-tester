#ifndef HOMING_H
#define HOMING_H

#include <cmath>
#include <cstdint>

// M3 FR-001 / FR-008, TC-006 / TC-007: pure staged homing FSM for the
// tensile tester (specs/M3-tensile-test-cycle/plan.md section 5). No
// Arduino, no heap, no millis(): the runner injects position, debounced
// endstop booleans (D4 debounce lives in src/Endstops, NOT here) and a
// monotonic ms counter through tick(). The model only decides; it never
// asserts hardware behaviour.
//
// Bottom (datum) homing: BackOff -> SeekDown -> FinalSeek -> Done.
// Top positioning (FR-008): SeekUp -> BackOffDown -> Done, never a datum.
//
// Rules (pinned by test/test_homing_model.cpp):
//   - a guarded distance D counts as CONSUMED once the measured travel
//     reaches D - oneStepMm; the one-step tolerance absorbs step
//     quantisation of the injected positions, so an ideal run's stage-2
//     trigger lands exactly on the stage-1 trigger position (datum
//     identity within oneStepMm, TC-007) and legitimately parked carriages
//     never fault (FR-001).
//   - SeekDown: bottom trigger before the clearance is consumed =
//     InstantTrigger (broken wire, spec A2); at/after = FinalSeek.
//   - FinalSeek internally backs off upward at full speed, then re-seeks
//     downward at finalSpeedFraction. Bottom is not judged during the
//     internal back-off (bounce immunity, TC-007); on the down sub-stage a
//     re-trigger before the clearance is consumed = StuckSwitch, at/after =
//     Done with datumSetThisRun() true (the only datum source).
//   - SeekUp: no instant-trigger guard - an immediate D26 hit is a legal
//     result (carriage already at the top, plan section 5). BackOffDown
//     completes the run WITHOUT a datum (FR-008 / D12).
//   - SeekDown/FinalSeek/SeekUp expire timeoutMs after their phase entry
//     (strict: delta == timeoutMs has NOT expired); BackOff and BackOffDown
//     never time out. A switch trigger and a timeout on the same tick: the
//     trigger wins.
//   - topStable is only consulted in SeekUp; bottomStable only in SeekDown
//     and the FinalSeek down sub-stage. startFromTop() is how the runner
//     signals "D26 tripped during BackOff" (FR-001 note): the seek starts
//     immediately and backOffCappedByTopLimit() stays true for the report.
//   - start()/startTopSeek() are accepted only from Idle or Done (homing is
//     idempotent, FR-001); startFromTop() only from BackOff; all other
//     calls and all ticks outside a run are no-ops. Fault latches until
//     configure() - recovery is reset/reboot (M1 FR-009).
//   - tick() with a non-finite position ignores the whole tick; ticks in
//     Idle/Done/Fault are no-ops.
//   - configure() re-parameterises AND resets to Idle (BreakDetector
//     convention).
//
// Defaults are the spec values: clearance 2 mm and final speed 25 % of
// seek (FR-001), seek timeout 30 s (TC-006), one motor step =
// 2 mm / 21968 steps (plan section 1).
enum class HomePhase {
  Idle,
  BackOff,
  SeekDown,
  FinalSeek,
  SeekUp,
  BackOffDown,
  Done,
  Fault
};

enum class HomeFault { None, SeekTimeout, InstantTrigger, StuckSwitch };

// Bench-established switch repeatability allowance (2026-10-11): a real
// KW4-3Z-3 re-triggers up to ~0.05 mm short of the ideal round trip
// (release vs re-trigger asymmetry), so the two trigger-judgment guards
// (SeekDown instant-trigger, FinalSeek stuck-switch) must not sit at the
// exact clearance. 0.3 mm = 6x the observed variance; a broken wire
// still reads ~0 mm of travel and keeps faulting (A2).
static constexpr float kSwitchRepeatabilityMm = 0.3f;

class HomingModel {
 public:
  void configure(float clearanceMm, float finalSpeedFraction,
                 uint32_t timeoutMs, float oneStepMm) {
    clearanceMm_ = clearanceMm;
    finalSpeedFraction_ = finalSpeedFraction;
    timeoutMs_ = timeoutMs;
    oneStepMm_ = oneStepMm;
    reset();
  }

  void reset() {
    phase_ = HomePhase::Idle;
    fault_ = HomeFault::None;
    finalDown_ = false;
    backOffCapped_ = false;
    datumSet_ = false;
    pendingEntry_ = false;
    entryPosMm_ = 0.0f;
    entryMs_ = 0;
  }

  // Bottom homing (FR-001): legal from Idle or Done (idempotent re-home).
  void start() {
    if (!startable()) {
      return;
    }
    clearRun();
    phase_ = HomePhase::BackOff;
  }

  // D26 hit during BackOff (FR-001 note): start the seek immediately.
  void startFromTop() {
    if (phase_ != HomePhase::BackOff) {
      return;
    }
    phase_ = HomePhase::SeekDown;
    backOffCapped_ = true;
    pendingEntry_ = true;
  }

  // `home top` (FR-008): never sets a datum.
  void startTopSeek() {
    if (!startable()) {
      return;
    }
    clearRun();
    phase_ = HomePhase::SeekUp;
  }

  void tick(float positionMm, bool bottomStable, bool topStable,
            uint32_t elapsedMs) {
    if (!std::isfinite(positionMm)) {
      return;  // bad feedback: the whole tick is ignored
    }
    if (phase_ == HomePhase::Idle || phase_ == HomePhase::Done ||
        phase_ == HomePhase::Fault) {
      return;  // quiescent
    }
    if (pendingEntry_) {
      // Entry position/time for externally started phases are latched on
      // the first tick (start()/startFromTop()/startTopSeek() carry no
      // position); phases entered from a tick stamp it right there.
      entryPosMm_ = positionMm;
      entryMs_ = elapsedMs;
      pendingEntry_ = false;
    }

    switch (phase_) {
      case HomePhase::BackOff:
        if (consumed(clearanceMm_, positionMm - entryPosMm_)) {
          enter(HomePhase::SeekDown, positionMm, elapsedMs);
        }
        break;

      case HomePhase::SeekDown:
        if (bottomStable) {
          if (consumed(clearanceMm_ - kSwitchRepeatabilityMm,
                       entryPosMm_ - positionMm)) {
            enter(HomePhase::FinalSeek, positionMm, elapsedMs);
          } else {
            fail(HomeFault::InstantTrigger);
          }
        } else if (timedOut(elapsedMs)) {
          fail(HomeFault::SeekTimeout);
        }
        break;

      case HomePhase::FinalSeek:
        if (!finalDown_) {
          // Internal back-off (up, full speed): bottom is NOT judged here
          // (bounce immunity of the second touch, TC-007).
          if (consumed(clearanceMm_, positionMm - entryPosMm_)) {
            finalDown_ = true;
            entryPosMm_ = positionMm;  // down sub-stage origin; the timeout
                                       // window stays phase-wide
          } else if (timedOut(elapsedMs)) {
            fail(HomeFault::SeekTimeout);
          }
        } else if (bottomStable) {
          // The re-seek covers the clearance minus the switch
          // repeatability allowance: an earlier re-trigger is a stuck
          // switch, never a datum (FR-001 second stage).
          if (consumed(clearanceMm_ - kSwitchRepeatabilityMm,
                       entryPosMm_ - positionMm)) {
            datumSet_ = true;
            phase_ = HomePhase::Done;
          } else {
            fail(HomeFault::StuckSwitch);
          }
        } else if (timedOut(elapsedMs)) {
          fail(HomeFault::SeekTimeout);
        }
        break;

      case HomePhase::SeekUp:
        // No instant-trigger guard here (plan section 5).
        if (topStable) {
          enter(HomePhase::BackOffDown, positionMm, elapsedMs);
        } else if (timedOut(elapsedMs)) {
          fail(HomeFault::SeekTimeout);
        }
        break;

      case HomePhase::BackOffDown:
        if (consumed(clearanceMm_, entryPosMm_ - positionMm)) {
          phase_ = HomePhase::Done;  // top seek completion: no datum, ever
        }
        break;

      default:
        break;
    }
  }

  HomePhase phase() const { return phase_; }
  HomeFault fault() const { return fault_; }

  bool motionWanted() const {
    return phase_ == HomePhase::BackOff || phase_ == HomePhase::SeekDown ||
           phase_ == HomePhase::FinalSeek || phase_ == HomePhase::SeekUp ||
           phase_ == HomePhase::BackOffDown;
  }

  // -1 down, +1 up, 0 while quiescent.
  int direction() const {
    switch (phase_) {
      case HomePhase::BackOff:
        return +1;
      case HomePhase::SeekDown:
        return -1;
      case HomePhase::FinalSeek:
        return finalDown_ ? -1 : +1;
      case HomePhase::SeekUp:
        return +1;
      case HomePhase::BackOffDown:
        return -1;
      default:
        return 0;
    }
  }

  // 1.0 everywhere except the FinalSeek slow re-seek (FR-001).
  float speedFraction() const {
    return (phase_ == HomePhase::FinalSeek && finalDown_) ? finalSpeedFraction_
                                                          : 1.0f;
  }

  bool backOffCappedByTopLimit() const { return backOffCapped_; }
  bool datumSetThisRun() const { return datumSet_; }

 private:
  bool startable() const {
    return phase_ == HomePhase::Idle || phase_ == HomePhase::Done;
  }

  void clearRun() {
    fault_ = HomeFault::None;
    finalDown_ = false;
    backOffCapped_ = false;
    datumSet_ = false;
    pendingEntry_ = true;
  }

  void enter(HomePhase next, float positionMm, uint32_t elapsedMs) {
    phase_ = next;
    finalDown_ = false;
    entryPosMm_ = positionMm;
    entryMs_ = elapsedMs;
    pendingEntry_ = false;
  }

  void fail(HomeFault fault) {
    fault_ = fault;
    phase_ = HomePhase::Fault;
  }

  bool consumed(float distanceMm, float travelMm) const {
    return travelMm >= distanceMm - oneStepMm_;
  }

  bool timedOut(uint32_t elapsedMs) const {
    return elapsedMs - entryMs_ > timeoutMs_;
  }

  float clearanceMm_ = 2.0f;          // FR-001 default homing clearance
  float finalSpeedFraction_ = 0.25f;  // FR-001: 25 % of the seek speed
  uint32_t timeoutMs_ = 30000;        // TC-006 default seek timeout
  float oneStepMm_ = 2.0f / 21968.0f;  // one motor step in spindle mm
  HomePhase phase_ = HomePhase::Idle;
  HomeFault fault_ = HomeFault::None;
  bool finalDown_ = false;      // FinalSeek sub-stage: slow re-seek downward
  bool backOffCapped_ = false;  // set only by startFromTop()
  bool datumSet_ = false;       // set only by a FinalSeek down trigger
  bool pendingEntry_ = false;   // latch entry pos/ms on the next tick
  float entryPosMm_ = 0.0f;     // current phase/sub-stage origin
  uint32_t entryMs_ = 0;        // current phase entry stamp (timeout window)
};

#endif  // HOMING_H
