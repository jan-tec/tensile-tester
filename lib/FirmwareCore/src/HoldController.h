#ifndef HOLDCONTROLLER_H
#define HOLDCONTROLLER_H

#include <cmath>
#include <cstdint>

// M4 FR-002..FR-005 / TC-001: pure constant-force hold FSM (creep-style
// test). specs/M4-constant-force-hold/plan.md D2-D5. Same conventions as
// Homing.h: no Arduino, no heap, no millis(); the runner injects force,
// position, debounced endstops, device fault and a monotonic ms counter
// through tick(). The model only decides; it never asserts hardware.
//
// States: Idle -> Homing -> Approach -> Holding -> Done | Limit | Lost |
// Aborted. Approach internally distinguishes the compression transit
// (seek up to clearanceMm) from the contact seek (tension: up directly,
// compression: down) - FR-002. Error stays a device-level concern
// (M1 DeviceModel); the controller reports it as end reason Fault.
//
// Rules (pinned by test/test_hold_controller.cpp):
//   - start() accepted from Idle and every terminal phase; a start while
//     running is a no-op (the command layer owns exclusivity, FR-001).
//   - Homing: waits for homedDone (the runner reuses the M3 homing FSM,
//     FR-007). A homing fault arrives as deviceFault or via requestStop.
//   - Approach (tension): seek UP at the approach speed until |force| >=
//     50% |setpoint| (approach threshold == hold floor, FR-004). Approach
//     (compression): transit UP to clearanceMm first - D26 during the
//     transit is NOT a limit, it simply starts the downward seek from
//     there (FR-002) - then seek DOWN to contact.
//   - Endstops judge ONLY in the direction of travel: E2 (top) during
//     upward travel or E1 (bottom) during downward travel = guarded stop
//     Limit (US3). The opposite switch is ignored (a homed carriage
//     resting near the datum must not false-limit a tension hold).
//   - Holding entry: holdStartMs = nowMs on the first in-loop tick; that
//     tick emits holdStartTick and NO correction (jolt-free loop entry,
//     FR-002). Duration counts from contact; durationMs == 0 = until
//     requestStop (FR-005).
//   - Corrections (D3): deadband proportional, rate-limited - inside the
//     tolerance band around |setpoint|: 0; outside:
//     sign(setpoint) * min(kGainMmPerN * |err|, kRateMmPerTick).
//     The rate limit (0.05 mm = the 30 mm/min machine cap) dominates
//     every large error, which is what makes the 100 ms force lag
//     stable (TC-003).
//   - Lost (FR-004): |force| below the floor (50% |setpoint|) on 2
//     CONSECUTIVE Holding ticks -> Lost (sample broke or slipped; the
//     machine must not chase the setpoint to a travel limit). A single
//     under-floor tick recovers without consequence.
//   - deviceFault any time after start -> Aborted(Fault) (M1 FR-009).
//   - requestStop() latches; the next tick ends Aborted(Stop).
//   - Non-finite force or position ignores the whole tick; ticks in
//     terminal phases are no-ops.
//
// Accumulators (TC-005, reported by T003/T006): holdTicks, inBandTicks,
// forceSumN, maxAbsDevN, plus hold-start position - mean force, max
// deviation, time-in-band and creep travel derive from them.
class HoldController {
 public:
  enum class Phase : uint8_t {
    Idle,
    Homing,
    Approach,
    Holding,
    Done,
    Limit,
    Lost,
    Aborted,
  };
  enum class End : uint8_t { None, Time, Stop, Limit, Lost, Fault };

  struct Targets {
    float setpointN;       // signed (tension +, compression -), pre-clamped
    uint32_t durationMs;   // 0 = until requestStop
    float tolerancePct;    // from TestConfig [1, 25]
    float clearanceMm;     // compression transit height [1, 100]
  };

  struct Inputs {
    bool homedDone;
    float forceN;
    float positionMm;
    bool endstopBottom;  // D25, pressed = true
    bool endstopTop;     // D26, pressed = true
    bool deviceFault;    // DeviceModel in Error (power FAIL, ...)
    uint32_t nowMs;
  };

  struct Output {
    Phase phase;
    End end;
    int seekDir;         // Approach: continuous seek direction (+1/-1, 0 none)
    float correctionMm;  // Holding: signed rate-limited relative move
    bool sample;         // record (force, position) this tick
    bool holdStartTick;  // first in-loop tick: switch motion mode, no jolt
  };

  // D3 constants (plan): correction rate cap and gain.
  static constexpr float kRateMmPerTick = 0.05f;  // = 30 mm/min cap
  static constexpr float kGainMmPerN = 0.002f;

  void start(const Targets &t) {
    if (phase_ == Phase::Homing || phase_ == Phase::Approach ||
        phase_ == Phase::Holding) {
      return;  // exclusivity belongs to the command layer (FR-001)
    }
    targets_ = t;
    phase_ = Phase::Homing;
    end_ = End::None;
    stopRequested_ = false;
    underFloorTicks_ = 0;
    holdTicks_ = 0;
    inBandTicks_ = 0;
    forceSumN_ = 0.0f;
    maxAbsDevN_ = 0.0f;
    holdStartMs_ = 0;
    holdStartPosMm_ = 0.0f;
    enteredHold_ = false;
    transitDone_ = false;
  }

  void requestStop() { stopRequested_ = true; }

  // True while a hold is in flight (pre-terminal phases) - feeds the
  // D11 exclusivity in TestRunner::active().
  bool running() const {
    return phase_ == Phase::Homing || phase_ == Phase::Approach ||
           phase_ == Phase::Holding;
  }

  Phase phase() const { return phase_; }
  End endReason() const { return end_; }

  // TC-005 report (valid whenever holdTicks() > 0; callers render it on
  // terminal phases). Derived pure - the accumulators live in the FSM.
  struct Report {
    float meanForceN;   // signed mean of the hold ticks
    float maxDevPct;    // max |deviation| as % of |setpoint|
    float inBandPct;    // % of hold ticks inside the tolerance band
    float creepMm;      // travel during the hold (positive = up)
    uint32_t holdTicks;
  };
  Report report() const {
    Report r{};
    r.holdTicks = holdTicks_;
    if (holdTicks_ == 0) {
      return r;
    }
    const float magnitude =
        targets_.setpointN < 0.0f ? -targets_.setpointN : targets_.setpointN;
    r.meanForceN = forceSumN_ / static_cast<float>(holdTicks_);
    r.maxDevPct = 100.0f * maxAbsDevN_ / magnitude;
    r.inBandPct = 100.0f * static_cast<float>(inBandTicks_) /
                  static_cast<float>(holdTicks_);
    r.creepMm = endPosMm_ - holdStartPosMm_;
    return r;
  }

  // Accumulators (valid after the run; TC-005 reads them).
  uint32_t holdTicks() const { return holdTicks_; }
  uint32_t inBandTicks() const { return inBandTicks_; }
  float forceSumN() const { return forceSumN_; }
  float maxAbsDevN() const { return maxAbsDevN_; }
  float creepTravelMm() const { return endPosMm_ - holdStartPosMm_; }

  Output tick(const Inputs &in) {
    Output out{};
    out.phase = phase_;
    out.end = end_;

    if (phase_ == Phase::Idle || isTerminal(phase_)) {
      return out;  // no-op (terminal latches; report() is pull-based)
    }
    if (!std::isfinite(in.forceN) || !std::isfinite(in.positionMm)) {
      return out;  // ignore the tick entirely (Homing convention)
    }
    // Live creep: the report stays readable DURING a hold (web status,
    // bench progress), not only after finish().
    endPosMm_ = in.positionMm;
    if (in.deviceFault) {
      finish(Phase::Aborted, End::Fault, in);
      out.phase = phase_;
      out.end = end_;
      return out;
    }
    if (stopRequested_) {
      finish(Phase::Aborted, End::Stop, in);
      out.phase = phase_;
      out.end = end_;
      return out;
    }

    const float magnitude = targets_.setpointN < 0.0f ? -targets_.setpointN
                                                      : targets_.setpointN;
    const int travelDir = targets_.setpointN < 0.0f ? -1 : 1;

    switch (phase_) {
      case Phase::Homing: {
        if (in.homedDone) {
          phase_ = Phase::Approach;
          out.phase = Phase::Approach;
          out.sample = false;
          // Fall through to the approach decision on the SAME tick so
          // the runner receives a seek direction immediately.
          approachOutput(in, travelDir, out);
        }
        break;
      }
      case Phase::Approach: {
        approachOutput(in, travelDir, out);
        break;
      }
      case Phase::Holding: {
        holdOutput(in, magnitude, travelDir, out);
        break;
      }
      default:
        break;
    }
    return out;
  }

 private:
  static bool isTerminal(Phase p) {
    return p == Phase::Done || p == Phase::Limit || p == Phase::Lost ||
           p == Phase::Aborted;
  }

  void finish(Phase p, End e, const Inputs &in) {
    endPosMm_ = in.positionMm;
    phase_ = p;
    end_ = e;
  }

  // Approach decision: compression transits up to clearanceMm first
  // (D26 during transit just ends the transit, FR-002), then seeks to
  // contact; tension seeks up immediately. Contact = |force| >= floor.
  void approachOutput(const Inputs &in, int travelDir, Output &out) {
    out.sample = true;  // FR-006: sampling through approach
    const float magnitude =
        targets_.setpointN < 0.0f ? -targets_.setpointN : targets_.setpointN;
    const float floorN = 0.5f * magnitude;

    if (travelDir < 0 && !transitDone_) {
      // Compression transit: upward, bounded by clearance or D26.
      if (in.endstopTop || in.positionMm >= targets_.clearanceMm) {
        transitDone_ = true;  // start the downward seek from here
        out.seekDir = -1;
      } else {
        out.seekDir = +1;
      }
      return;
    }

    const int seekDir = travelDir;  // tension: up; compression: down
    out.seekDir = seekDir;
    // Guarded stop: endstop in the direction of travel (US3). The
    // opposite switch is ignored (datum-adjacent carriages).
    const bool travelEndstop = seekDir > 0 ? in.endstopTop : in.endstopBottom;
    if (travelEndstop) {
      out.seekDir = 0;
      finish(Phase::Limit, End::Limit, in);
      out.phase = phase_;
      out.end = end_;
      return;
    }
    const float forceMagnitude = in.forceN < 0.0f ? -in.forceN : in.forceN;
    if (forceMagnitude >= floorN) {
      // Contact: close the loop jolt-free - no correction this tick.
      phase_ = Phase::Holding;
      holdStartMs_ = in.nowMs;
      holdStartPosMm_ = in.positionMm;
      enteredHold_ = true;
      out.phase = Phase::Holding;
      out.seekDir = 0;
      out.holdStartTick = true;
    }
  }

  void holdOutput(const Inputs &in, float magnitude, int travelDir,
                  Output &out) {
    out.sample = true;
    if (!enteredHold_) {
      // Defensive: Holding is only entered through approachOutput.
      enteredHold_ = true;
      holdStartMs_ = in.nowMs;
      holdStartPosMm_ = in.positionMm;
      out.holdStartTick = true;
    }

    // Duration (FR-005): counts from contact; 0 = until stop.
    if (targets_.durationMs != 0 &&
        in.nowMs - holdStartMs_ >= targets_.durationMs) {
      finish(Phase::Done, End::Time, in);
      out.phase = phase_;
      out.end = end_;
      return;
    }

    const float forceMagnitude = in.forceN < 0.0f ? -in.forceN : in.forceN;
    const float floorN = 0.5f * magnitude;
    const float toleranceN = magnitude * targets_.tolerancePct / 100.0f;
    const float dev = forceMagnitude - magnitude;  // signed deviation
    const float absDev = dev < 0.0f ? -dev : dev;

    holdTicks_++;
    forceSumN_ += in.forceN;
    if (absDev > maxAbsDevN_) {
      maxAbsDevN_ = absDev;
    }

    // Lost (FR-004): under the floor on 2 consecutive ticks.
    if (forceMagnitude < floorN) {
      if (++underFloorTicks_ >= 2) {
        finish(Phase::Lost, End::Lost, in);
        out.phase = phase_;
        out.end = end_;
        return;
      }
    } else {
      underFloorTicks_ = 0;
    }

    // Deadband + rate-limited correction (D3).
    if (absDev <= toleranceN) {
      inBandTicks_++;
      out.correctionMm = 0.0f;
    } else {
      const float err = -dev;  // underforce -> positive (outward)
      const float step = err * kGainMmPerN;
      const float capped = step > kRateMmPerTick
                               ? kRateMmPerTick
                               : (step < -kRateMmPerTick ? -kRateMmPerTick
                                                         : step);
      out.correctionMm = static_cast<float>(travelDir) * capped;
    }
  }

  Phase phase_ = Phase::Idle;
  End end_ = End::None;
  Targets targets_{};
  bool stopRequested_ = false;
  uint8_t underFloorTicks_ = 0;
  bool enteredHold_ = false;
  bool transitDone_ = false;
  uint32_t holdStartMs_ = 0;
  float holdStartPosMm_ = 0.0f;
  float endPosMm_ = 0.0f;
  uint32_t holdTicks_ = 0;
  uint32_t inBandTicks_ = 0;
  float forceSumN_ = 0.0f;
  float maxAbsDevN_ = 0.0f;
};

#endif  // HOLDCONTROLLER_H
