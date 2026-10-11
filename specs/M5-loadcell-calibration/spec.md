# Feature Specification: M5 - Load Cell Calibration (Guided Wizard + Manual Path)

Status: Draft (rev 2 per #82) | Rigor: Spec-Anchored | Created: 2026-10-07 |
Depends on: M3 (cycle infrastructure), M6 (JSON response envelope)

**Goal:** Operator-friendly load cell calibration: an interactive guided
wizard (enter your mass, hang it on, take it off, done) plus a one-shot
manual command for hosts and experts, both guarded by a physics-derived
plausibility window so a mis-entered mass cannot silently corrupt the
calibration. Weight-sizing guidance lives in documentation, not software
(owner decision #82).

## Revision note (rev 1 -> rev 2, #82)

Rev 1 specified a software weight suggestion (`cal_suggest`, a persistent
max-force register, data-driven 80% basis, suggestion cross-check
warning). The project owner redirected: the suggestion moves to
documentation; the software provides a guided interactive flow instead.
Dropped in rev 2: `cal_suggest`, the max-force register, `cal_reset_max`,
the suggestion cross-check warning. Kept: the pinned two-step commands
(underlying primitives), the physics-window `Implausible` verdict
(suggestion-independent), validity reporting. Residual risk accepted with
the owner's redirect: only 10x/0.1x mass typos are rejected by the physics
window; factor-2 errors are not detected (the dropped cross-check was the
only 2x guard).

## User Scenarios

### US1 (P1) - Guided calibration wizard

As an operator, I calibrate with the mass I have, prompted step by step.

- **Given** the machine idle, **When** I run `calibrate 50000`, Then the
  machine prompts "hang 50000 g (50.0 kg) now, then send `cal_next`",
  waits, averages a settled force window after confirmation, prompts
  "remove the mass, then send `cal_next`", verifies the reading returns to
  the zero reference, computes the factor, applies the physics verdicts,
  saves, and reports the result - every prompt and the result as one JSON
  line (M6 envelope).
- **Given** the wizard is waiting for the mass, **When** I send `stop`,
  Then the wizard aborts without touching the stored calibration.

### US2 (P1) - One-shot manual path

As a host computer (or expert operator), I calibrate non-interactively.

- **Given** the mass is already hanging, **When** I run `cal_zero` then
  `cal_apply 50000`, Then the factor computes and persists exactly as the
  M1-era behavior pinned in FR-003 - one command, one JSON response, no
  prompts. This is the path automation uses.

### US3 (P2) - Persistence, validity, documentation

- **Given** a completed calibration, **When** the machine reboots, Then
  factor + valid flag are restored from NVS and the boot selftest NVS check
  reflects them (M1 reuse).
- **Given** an uncalibrated machine, **When** force is read, Then the
  reading is flagged invalid rather than presented as newtons.
- **Given** the operator asks "what mass should I use?", **When** they open
  `docs/calibration.md`, Then they find the sizing guidance (near the
  working load, roughly 80% of the highest expected force, mass accuracy
  dominates the result, traceability caveat).

## Requirements

### Functional

- **FR-001 (Guided wizard):** `calibrate <grams>` starts the wizard:
  grams > 0 required. The machine is unloaded at that moment, so the
  wizard auto-zeros on start - this defines the zero reference that
  Measuring uses and ZeroCheck verifies. States: PromptHang -> Measuring
  -> PromptRemove -> ZeroCheck -> Done, with `cal_next` as the operator
  confirmation between prompts and `stop` aborting cleanly (stored
  calibration untouched until Done). Measuring averages a settled window
  (10 Hz samples; settle = spread below a documented threshold across the
  window). ZeroCheck re-reads the no-load reference and warns if it
  drifted beyond a documented band (re-tare hint). Prompts and the final
  result render as M6 JSON lines; `cal_next`/`stop` outside the wizard
  are normal commands.
- **FR-002 (Manual one-shot path):** `cal_zero` tares at no load;
  `cal_apply <grams>` requires grams > 0, computes
  `factor = (averageRaw - offset) / grams`, applies the verdicts below,
  and persists a valid factor in NVS namespace `testmachine`. Both commands
  stay non-interactive (host contract, M6).
- **FR-003 (Physics plausibility window):** every computed factor (wizard
  and manual) must sit inside [0.3x, 3x] of the physics-derived nominal,
  else the verdict is `Implausible`, nothing is persisted, and the error
  says so. Nominal derivation (HX711 datasheet: full-scale differential
  range = +/-0.5 x AVDD/GAIN, 24-bit two's complement):
  `counts/mV = (2^23 - 1) x GAIN / (500 x AVDD)`;
  `nominal counts/g = sensitivity mV/V x excitation V x counts/mV /
  capacity g` -> ~8.59 counts/g for the YZC-516C (2.0 mV/V, 5 V, 500 kg).
  Window is wide (real excitation varies 4.3-5 V in practice); decimal-
  place typos in the mass (10x / 0.1x entries) always land outside it.
  Verdict set: Valid / Zero / Negative (per NQP #46, unchanged) /
  Implausible (appended, never renumbered).
- **FR-004 (One-point linear model):** one zero point + one span point
  (linear). YZC-516C nonlinearity (about 0.02-0.05% FS) makes multi-point
  mapping unnecessary at this tolerance class. No two-point scope.
- **FR-005 (Validity reporting):** `pos`, `force`, and the M6 state object
  carry the calibration-valid flag; uncalibrated force readings are marked
  invalid, never silently presented as newtons.
- **FR-006 (Documentation deliverable):** `docs/calibration.md` carries the
  weight-sizing guidance moved out of software per #82: calibrate near the
  working load (roughly 80% of the highest expected force), the mass's own
  accuracy dominates the result, no certified-standard traceability is
  claimed on this bench, and the abort/retry etiquette of the wizard.

### Constraints & TDD Targets

- **TC-001:** wizard state machine (Idle -> PromptHang -> Measuring ->
  PromptRemove -> ZeroCheck -> Done | Aborted) is a pure `FirmwareCore`
  model, host-tested RED-first, including stop-at-every-state.
- **TC-002:** settled-window detection is a pure function (sample stream
  in, settled verdict + averaged value out), host-tested on synthetic
  stable/driving/glitzy series.
- **TC-003:** physics window: pure derivation of nominal counts/g from
  (sensitivity mV/V, excitation V, gain, capacity g) -> ~8.59 for the
  YZC-516C; host-tested verdict table - correct mass accepted, 10x/0.1x
  mass entries rejected Implausible; existing Valid/Zero/Negative
  expectations unchanged (NQP #46 pins remain green).
- **TC-004:** NVS persistence and boot restore stay target-tested (house
  host/target boundary).

### Non-Functional

- No new libraries; wizard prompts are short ASCII lines (M6 envelope for
  the JSON response, prompt text inside `data`).
- NVS writes: one per successful calibration - negligible wear.
- The wizard must not run concurrently with a cycle (cycle exclusivity, M3
  FR-005 rule).

## Key Entities & Data Model

- **CalWizard:** state, target grams, averaged raw, zero drift - RAM only.
- **Calibration:** factor, valid flag - NVS namespace `testmachine`
  (unchanged).

## Assumptions & Handoffs

- **A1:** bench reference weights are ordinary masses, not certified
  standards: traceability (OIML R60 / ISO 376 / ASTM E74) is NOT claimed.
  Absolute accuracy equals the mass's own accuracy - documented in
  FR-006's page.
- **A2:** the wizard is a serial interaction; a web-button wizard is web
  UI scope (#83) and not this milestone.
- **A3:** settle threshold and zero-drift band are bench constants with
  documented defaults; refined at bring-up like the M3 thresholds.

## Success Criteria

- **SC-001:** `calibrate 50000` walks an operator through hang -> next ->
  remove -> next with prompts, and the force reads correctly afterwards
  (repeat-readings stable, factor inside the physics window).
- **SC-002:** `cal_zero` + `cal_apply 50000` performs the same calibration
  non-interactively for a host script.
- **SC-003:** `cal_apply 5000` (decimal typo) is rejected Implausible with
  nothing persisted; a reboot preserves the last valid calibration.
- **SC-004:** `docs/calibration.md` answers "what mass should I use?"
  without any software suggestion feature.
