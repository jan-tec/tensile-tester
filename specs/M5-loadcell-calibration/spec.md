# Feature Specification: M5 - Load Cell Calibration + Weight Suggestion

Status: Draft | Rigor: Spec-Anchored | Created: 2026-10-07 | Depends on: M3,
M4 (run summaries feed the suggestion)

**Goal:** Formalize the existing two-step load cell calibration (tare at
zero, span with a known mass) as a spec, and add a data-driven weight
suggestion: the machine proposes a calibration mass of about 80% of the
highest force it has actually measured, while the operator stays free to
apply any mass.

## User Scenarios

### US1 (P1) - Guided weight suggestion

As an operator, I ask the machine for a calibration weight suggestion
instead of guessing.

- **Given** the machine has completed at least one test or hold cycle,
  **When** I run `cal_suggest`, Then it reports the suggested mass in grams
  (about 80% of the highest recorded force, converted via 9.80665 m/s^2 and
  rounded to the nearest 100 g), the force it is derived from, and a one-
  line rationale.
- **Given** no cycle has ever completed (register empty), **When** I run
  `cal_suggest`, Then it reports that no suggestion is available and says
  so explicitly - it does NOT fall back to an arbitrary capacity-based
  number.

### US2 (P1) - Free mass choice

As an operator, I calibrate with any mass I have on the bench.

- **Given** the suggestion is 69300 g but the bench has a 50 kg weight,
  **When** I run `cal_apply 50000`, Then the calibration completes with
  that mass - the suggestion is advisory only.
- **Given** `cal_zero` was run at no load, **When** `cal_apply` computes
  the factor, Then the existing verdicts hold (valid, zero, negative
  rejected per NQP #46) and a valid factor persists in NVS.

### US2 (P2) - Persistence and validity

As an operator, calibration survives reboots and is visibly valid.

- **Given** a completed calibration, **When** the machine reboots, Then the
  factor and valid flag are restored from NVS and the boot selftest NVS
  check reflects them (M1 reuse).
- **Given** an uncalibrated machine, **When** force is read, Then the
  machine reports the calibration as invalid rather than presenting raw
  counts as newtons.

## Requirements

### Functional

- **FR-001 (Max-force register):** the machine maintains a persistent
  highest-recorded-force register (N, magnitude). It updates from the run
  peak exactly once per completed cycle (M3 `test`, M4 `chold` - at cycle
  end, never per sample: no NVS wear at 10 Hz). Monotonic: only a higher
  peak overwrites it. A `cal_reset_max` command clears it.
- **FR-002 (Suggestion):** `cal_suggest` computes
  `grams = round100(0.8 x maxForceN / 9.80665 x 1000)` from the register,
  reports grams + source force + rationale; with an empty register it
  reports unavailability (US1). Pure math, host-tested (TC-001).
- **FR-003 (Two-step calibration, pinned + guarded):** `cal_zero` tares at
  no load; `cal_apply <grams>` requires grams > 0, computes
  `factor = (averageRaw - offset) / grams` and applies four verdicts:
  Valid / Zero / Negative (per NQP #46, unchanged) plus **Implausible** -
  the factor magnitude must sit inside a physics window derived from the
  load cell constants: counts/mV = (2^23 - 1) x GAIN / (500 x AVDD)
  (HX711 datasheet: full-scale differential range = +/-0.5 x AVDD/GAIN,
  24-bit two's complement), nominal counts/g = sensitivity mV/V x
  excitation V x counts/mV / capacity g -> ~8.59 counts/g for the
  YZC-516C (2.0 mV/V, 5 V, 500 kg). Accept window [0.3x, 3x] of nominal
  (wide because real excitation varies 4.3-5 V in practice); outside ->
  rejected Implausible, NVS untouched. Decimal-place typos in the mass
  (10x / 0.1x entries) always land outside the window. Additionally the
  result carries a `warning` field when the applied mass differs from the
  `cal_suggest` basis by more than 2x - advisory only, calibration still
  succeeds (operator freedom per US2). Valid factors persist in NVS
  namespace `testmachine`.
- **FR-004 (One-point linear model):** the calibration model stays one
  zero point + one span point (linear). YZC-516C nonlinearity (about
  0.02-0.05% FS) makes multi-point mapping unnecessary at this machine's
  tolerance class. No two-point/multi-point scope.
- **FR-005 (Validity reporting):** `pos`, `force`, and the M6 state object
  carry the calibration-valid flag; uncalibrated force readings are marked
  invalid, never silently presented as newtons.

### Constraints & TDD Targets

- **TC-001:** suggestion math is a pure `FirmwareCore` function
  (maxForceN -> grams): known inputs (850 N -> 69300 g), rounding edge
  (exact 100 g boundary), register-empty case. Host-tested RED-first.
- **TC-002:** register update policy: peak update happens once per cycle
  end, monotonic-only, `cal_reset_max` clears - pure logic host-tested.
- **TC-003:** verdict mapping (Valid/Zero/Negative) is already pinned by
  M1-era host tests (NQP #46) - this spec must not change those tests'
  expectations.
- **TC-004:** NVS persistence and boot restore stay target-tested (house
  host/target boundary).
- **TC-005:** implausibility window: pure derivation of nominal counts/g
  from (sensitivity mV/V, excitation V, gain, capacity g) -> ~8.59 for the
  YZC-516C; host-tested verdict table - correct mass accepted, 10x/0.1x
  mass entries rejected Implausible, 2x entry accepted with warning vs the
  suggestion basis; existing Valid/Zero/Negative expectations unchanged
  (NQP #46 pins remain green).

### Non-Functional

- No new libraries; no new hardware beyond whatever reference mass the
  operator owns.
- NVS writes: one per completed cycle (register) + one per calibration
  apply - negligible wear.

## Key Entities & Data Model

- **CalRegister:** maxForceN (uint16, 0.1 N resolution enough? - no: plain
  N float, cleared by `cal_reset_max`) - NVS-persisted.
- **Suggestion:** grams, sourceForceN, rationale - derived, not persisted.

## Assumptions & Handoffs

- **A1:** bench reference weights are ordinary masses, not certified
  standards: traceability (OIML R60 / ISO 376 / ASTM E74) is NOT claimed.
  The machine reports force in N relative to whatever mass the operator
  applied; absolute accuracy is the mass's own accuracy.
- **A2:** suggestion basis 80% of the working maximum minimizes relative
  span error at the loads actually used (calibration point near working
  load); capacity-based (about 400 kg) is impractical on a bench.
- **A3:** response rendering (JSON envelope) follows M6 when it lands; if
  M5 ships first, `cal_suggest` echoes human-readable text carrying the
  same three fields.
- **Next Step:** `/spec.plan` together with M6 (shared JSON response
  contract).

## Success Criteria

- **SC-001:** after a test that peaked at 850 N, `cal_suggest` proposes
  69300 g and the rationale names the 850 N source.
- **SC-002:** `cal_apply 50000` with a 50 kg bench weight calibrates
  successfully (factor inside the physics window, no warning vs the
  suggestion basis); repeated force readings at the same load are stable
  and track the applied mass within that mass's own stated accuracy.
- **SC-003:** a reboot preserves the calibration; the valid flag is visible
  in status output; `cal_reset_max` empties the register and `cal_suggest`
  then reports unavailability.
