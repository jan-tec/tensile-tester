# Feature Specification: M4 - Constant-Force Hold (Creep-Style Test)

Status: Draft | Rigor: Spec-Anchored | Created: 2026-10-07 | Depends on: M3
(homing, sampling, USB data path)

**Goal:** Apply and hold a constant force setpoint on the sample by
closed-loop motion control, for a configured duration or until the operator
interrupts. Both loading directions are supported (tension = positive,
compression = negative setpoint). The full force/position time series is
served over the USB serial line as CSV (M3 data path).

## User Scenarios

### US1 (P1) - One-command constant-force hold

As an operator, I command a force setpoint and duration, and the machine
reaches and holds that force hands-free.

- **Given** a clamped sample, **When** I run `chold 50 5`, Then the machine
  homes, approaches the sample without jolts until the force reaches the
  approach threshold (50% of setpoint), closes the control loop, holds the
  force at 50 N within the configured tolerance for 5 minutes, stops, and
  reports the hold summary (mean force, achieved deviation, duration, end
  reason).
- **Given** the same command without a duration (`chold 50`), **When** the
  operator sends `stop`, Then the hold ends immediately and all recorded
  data stays retrievable.
- **Given** a negative setpoint (`chold -50`), **When** the machine runs the
  cycle, Then it transits up by the compression clearance, approaches
  downward onto the sample, loads it in compression (presses down), and
  holds -50 N by the same loop rules.

### US2 (P1) - Data retrieval

As a host computer, I retrieve the hold time series as CSV over USB
(M3 path).

- **Given** a completed or aborted hold, **When** I send `data`, Then the
  machine streams the series as CSV lines (index, displacement mm, force N)
  exactly as M3; plotting force and position vs time shows the approach,
  the settling, and the hold band.

### US3 (P2) - Safety and sample loss

As an operator, I am protected against runaway motion when the sample
yields, breaks, or slips during the hold.

- **Given** the sample breaks or slips during a tensile hold, **When** the
  force collapses below the hold floor while the carriage keeps traveling,
  Then the loop aborts (end reason `lost`), the motor stops, and data stays
  retrievable - the machine must not chase the setpoint to a travel limit.
- **Given** the carriage reaches D26 (tension) or D25 (compression) during
  approach or hold, **Then** the run stops as a guarded stop (end reason
  `limit`) - both endstops are hard bounds in this milestone.
- **Given** the 5 V rail monitor enters FAIL, **When** the hold is active,
  Then the run aborts with the driver disabled (M1 FR-005/FR-009 reuse).

## Requirements

### Functional

- **FR-001 (Command surface):** `chold <N> [minutes]` starts the cycle
  (signed setpoint: positive = tension/pull up, negative =
  compression/press down; magnitude clamped to a documented safe range,
  floor above the ~1 N noise floor); `choltol <pct>` sets the hold tolerance
  (default 5%, persisted, clamped 1-25%); `choldclear <mm>` sets the
  compression transit height above the homing datum (default 20 mm,
  persisted, clamped to the travel range). Duration in minutes with one
  decimal place (6 s resolution), 0 or omitted = until operator `stop`.
  Cycles are exclusive: `chold` (like M3 `test`) while another cycle is
  active is rejected with an actionable message; the running cycle is
  unaffected, and `stop` always works. All M3 serial commands stay valid.
- **FR-002 (Approach, jolt-free):** a tension run (positive setpoint)
  approaches directly: after homing, the machine moves upward at the
  configured approach speed (reuses `testspeed`) until the force magnitude
  reaches the approach threshold (50% of |setpoint|). A compression run
  (negative setpoint) first transits upward from the homing datum by the
  compression clearance (`choldclear`, FR-001; bounded by D26 - if D26
  trips during transit, the approach starts immediately from there), then
  approaches downward until force contact. In both cases the machine then
  decelerates into the control loop without overshooting the setpoint by
  more than the approach overshoot bound (TC-002). Load application without
  jolts per ISO 899-1 practice.
- **FR-003 (Control loop):** while holding, the loop corrects position to
  keep force magnitude within the tolerance band around |setpoint|.
  Corrections are rate-limited (max travel per 10 Hz tick bounded - TC-003)
  because the force feedback lags (HX711 100 ms). Loop decisions are
  host-testable pure logic (TC-001).
- **FR-004 (Hold floor / sample loss):** if the force magnitude falls below
  the hold floor (default: 50% of setpoint) while the loop is actively
  correcting outward, the run aborts with end reason `lost` (sample broke or
  slipped). The floor is the same threshold as the approach threshold; the
  collapse must persist for 2 consecutive 10 Hz ticks (200 ms) to confirm -
  a single glitch sample cannot abort a hold.
- **FR-005 (Duration + end reasons):** the hold ends by `time` (duration
  reached), `stop` (operator), `limit` (D25/D26 guarded stop, no machine
  code - M3 FR-002 semantics), `lost` (FR-004), or `fault` (power FAIL,
  M1 reuse). The summary reports end reason, held duration, mean force,
  max deviation, time-in-band (percentage of hold ticks within tolerance),
  and travel during hold (creep displacement).
- **FR-006 (Sampling + buffer):** sampling at the HX711 native 10 Hz
  continues through approach and hold into the M3 buffer. A hold runs to
  its configured duration regardless of buffer size: when the buffer fills,
  the oldest samples are overwritten (keep-last ring policy) and the
  overwrite count is reported. Resolution trade-offs for very long holds
  (progressive decimation) are a plan decision.
- **FR-007 (Position reference):** the M3 homing datum applies unchanged
  (clearance back-off, seek, instant-trigger guard, D26 bound during
  back-off). Compression runs reference the same datum; positive position
  is always "up".

### Constraints & TDD Targets

- **TC-001:** hold controller as a pure `FirmwareCore` class (force samples
  + limits in, position corrections out; state machine
  Idle -> Homing -> Approach -> Holding -> Done | Limit | Lost | Aborted |
  Error), host-tested RED-first including the force-collapse (Lost) and
  limit transitions.
- **TC-002:** approach overshoot bound: with the documented correction rate
  limit and worst-case 2-sample force latency, first-peak force <=
  |setpoint| + overshoot bound (default +10%, host-tested on the pure
  model, bench-verified).
- **TC-003:** correction rate limit: max carriage travel per 10 Hz tick
  (default 0.1 mm/tick = 60 mm/min ceiling) is a pure, host-tested clamp -
  it is what makes the 100 ms force lag stable.
- **TC-004:** ring keep-last policy: pure buffer host test - after N > 8192
  pushes, the buffer holds the newest 8192 samples and the overwrite count
  equals N - 8192.
- **TC-005:** tolerance accounting: mean force, max deviation, and
  time-in-band are computed by pure functions, host-tested on synthetic
  series (in-band, drifting, and noisy cases).

### Non-Functional

- No new libraries; no new hardware.
- WiFi status serving continues during a hold (view-only, no change to the
  M3 web surface).
- One run in RAM shared with M3 (starting a hold discards a previous tensile
  run's data and vice versa - A3 semantics of M3 apply).

## Key Entities & Data Model

- **HoldConfig:** setpoint N (signed), duration minutes, tolerance pct,
  compression clearance mm - persisted (tolerance, clearance) /
  per-command (setpoint, duration).
- **HoldRun:** mode (tension/compression), end reason, held duration s, mean
  force N, max deviation N/pct, travel during hold mm, overwrite count -
  extends the M3 TestRun model.
- **TestSample / buffer:** identical to M3 (10 Hz fixed interval, 8192
  capacity, keep-last for this mode).

## Assumptions & Handoffs

- **A1:** compression needs a mechanical arrangement the firmware cannot
  define: a platen/anvil and sample fixture positioned inside the
  compression window (between the datum and the `choldclear` transit
  height). Hardware task for the operator (mirrors #76); firmware is
  direction-agnostic (sign convention + transit height only).
- **A2:** force sign convention: tare establishes zero; tension (pull up)
  reads positive, compression reads negative. Bench-verify the sign after
  wiring per the existing calibration path (`cal_zero`/`cal_apply`).
- **A3:** hold durations are bench-scale (minutes), not standards-scale
  (ISO 899-1 cites 1000 h tests): temperature and humidity logging are out
  of scope; the machine reports elapsed time only.
- **A4:** loop tuning (gains/algorithm shape behind the rate-limited
  corrections) is plan-level; the spec pins the measurable contract
  (tolerance, overshoot bound, jolt-free approach).
- **Next Step:** `/spec.plan` after M3 spec approval (shared data path
  contracts land there first).

## Success Criteria

- **SC-001:** on the bench, `chold 50 5` reaches and holds 50 N within the
  configured tolerance for 5 minutes and stops by itself; the summary
  reports mean force within tolerance.
- **SC-002:** `chold -50` performs the same cycle in compression on the
  bench arrangement (A1).
- **SC-003:** cutting the sample mid-hold ends the run with end reason
  `lost`, motor stopped, no travel-limit contact; CSV shows the collapse.
- **SC-004:** the CSV of a hold plots force vs time with the approach,
  settle-in, and in-band hold visible, and position vs time shows the creep
  travel the loop applied.
