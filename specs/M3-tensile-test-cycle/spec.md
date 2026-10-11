# Feature Specification: M3 - Automated Tensile Test Cycle

Status: Draft | Rigor: Spec-Anchored | Created: 2026-10-07 | Depends on: M1, #71 (FastAccelStepper)

**Goal:** One-command tensile test: home on the lower endstop, pull the sample
upward at a configured test speed until it breaks, detect the break from the
force signal, stop safely, and serve the full force-displacement series to a
host computer over the USB serial line as CSV for plotting.

## User Scenarios

### US1 (P1) - One-command test cycle

As an operator, I start a test with one command and the machine completes the
cycle without further interaction.

- **Given** a fresh sample clamped in the machine, **When** I run `test`, Then
  the machine homes down onto the bottom endstop (D25), pulls upward at the
  configured test speed, records force and position at 10 Hz, stops the motor
  at break detection, and reports the result summary.
- **Given** the sample does not break, **When** the sample buffer is full or
  the operator sends `stop`, Then the pull stops and all recorded data stays
  retrievable.

### US2 (P1) - Data retrieval via USB

As a host computer, I retrieve the sampled measurements as CSV over the USB
serial line for plotting.

- **Given** a completed (or aborted) test with samples in memory, **When** I
  send `data` over serial, Then the machine streams the series as CSV lines
  (index, displacement mm, force N) and an end marker; a chunked read
  (`data <from-index>`) resumes an interrupted dump without duplicates.
- **Given** the same test data, **When** I plot force vs displacement from
  the CSV, Then the curve shows the elastic region, the force peak, and the
  drop at break.

### US3 (P2) - Safety and fault paths

As an operator, I am protected against endstop faults and runtime power loss.

- **Given** the bottom endstop (D25) never triggers during homing, **When**
  the seek timeout expires, Then homing aborts, the driver is disabled, and
  the fault is reported with a stable machine code (Error state per M1
  FR-009).
- **Given** a test is pulling and the sample does not break, **When** the
  carriage reaches the upper travel limit (D26), Then the motor stops
  immediately (hard backstop), the run is marked `limit`, and recorded data
  stays retrievable. The machine does not enter the Error state for a limit
  trip - it is a guarded stop, not a fault.
- **Given** a test is running, **When** the 5 V rail monitor enters FAIL
  (M1 FR-005), Then the test aborts, the driver is disabled, and recorded
  data stays retrievable.

## Requirements

### Functional

- **FR-001 (Homing, two-stage per GRBL practice):** `home` first backs off
  upward by the homing clearance (configurable, default 2 mm; bounded by
  Endstop 2 - if D26 trips during back-off, the seek starts immediately
  from there), then seeks downward at the homing seek speed until the
  bottom endstop (Endstop 1, GPIO 25, COM+NC, pressed = HIGH) triggers,
  backs off upward by the clearance again, and re-seeks at the slow final
  speed (default 25% of seek speed) until the endstop re-triggers - the
  second touch defines the datum, switch bounce settled (GRBL two-stage
  homing + debounce-delay practice; operator request #81 "entprellen").
  The re-seek covers exactly the clearance distance: a re-trigger before
  it is consumed = stuck switch -> Error state (same instant-trigger
  rule, second stage).
  The datum is position zero (spindle mm). Because the clearance always
  precedes the seek, a trigger before the clearance is consumed =
  instant-trigger = broken wire -> Error state (never a phantom datum, and
  never a false fault for a carriage legitimately near the stop). Homing
  is idempotent: running it twice yields the same datum within one motor
  step.
- **FR-008 (Home top, positioning utility):** `home top` seeks upward at
  the seek speed until Endstop 2 (GPIO 26) triggers, then backs off
  downward by the clearance. It does NOT redefine the position datum -
  position zero stays the bottom datum (FR-006); `home top` exists for
  sample insertion at upper positions and full-travel verification, and
  reports the reached travel span when the bottom datum is known.
- **FR-002 (Test cycle):** `test` performs: (1) home on Endstop 1 if not
  already homed this session (an existing datum is kept - the operator may
  have inserted the sample after homing; #81), (2) arm
  the break detector, (3) pull upward at the configured test speed, (4) on
  break detection stop the motor (forceStop semantics), (5) keep sampling a
  fixed tail window (1 s) to capture the force drop, (6) report the summary
  (peak force, break displacement, sample count). Reaching the upper travel
  limit (Endstop 2, GPIO 26, COM+NC, triggers on rising edge) during pull
  stops the motion immediately (hard backstop) and marks the run `limit` -
  a guarded stop, not a fault. The limit trip is reported via run status
  only (summary, `testdata`); it carries no machine code.
- **FR-003 (Break detection):** the detector arms when force rises above the
  start threshold (default 50 N, about 1% of the 4903 N YZC-516C capacity);
  it tracks the running force peak; break = force falls below (drop
  fraction x running peak) (default 50%). Thresholds are commands with
  values persisted in NVS (namespace `testmachine`). Detection uses the
  unfiltered 10 Hz force sample; the displayed/low-pass value is unaffected.
- **FR-004 (Sampling):** during pull and tail, force + position pairs are
  recorded at the HX711 native 10 Hz on a fixed interval (timestamp implicit
  from index). Buffer capacity is bounded (8192 samples). On buffer full the
  pull stops safely and the data set is flagged truncated.
- **FR-005 (USB serial API):** new serial commands, shared command policy
  (`Commands::execute`, serial only - web API unchanged): `home`,
  `home top`, `test`, `testspeed <mm/min>` (persisted, clamped to the existing feed
  range), `testarm <N>` + `testdrop <pct>` (persisted), `data [from-index]`
  (CSV chunk dump), `testdata` (non-destructive status: state, sample count,
  capacity, peak, truncation flag). Existing `stop` aborts pull and homing.
  Cycle commands are exclusive: `test` while another cycle is active is
  rejected with an actionable message; the running cycle is unaffected.
- **FR-006 (Position datum):** position reads in mm upward from the endstop
  datum after the first successful homing; before that, position is unknown
  and `pos` reports it as such. `home` and `test` establish the datum
  themselves (FR-002); a failed in-cycle homing transitions to Error
  (FR-007), not to a rejection path.
- **FR-007 (Fault integration):** endstop seek timeout and instant-trigger
  violations produce new stable machine codes (appended, never renumbered -
  `ErrorCode.h` rule); a runtime power FAIL aborts the test with driver off
  (M1 FR-005/FR-009 reuse). The endstops join the boot selftest as
  `CHECK ENDSTOP1` / `CHECK ENDSTOP2`: NC wiring reads LOW when open, so
  HIGH at boot = WARN "pressed or broken wire - jog off the switch and
  re-run selftest" (a carriage legitimately parked on a switch also reads
  HIGH - operator judgment, hence WARN not FAIL).

### Constraints & TDD Targets

- **TC-001:** test cycle state machine (Idle -> Homing -> Pulling -> Tail ->
  Done | Limit | Aborted | Error) is a pure `FirmwareCore` model, host-tested
  RED-first (transition table incl. power-fail, top-limit and stop
  interrupts).
- **TC-002:** break detector is a pure function/class (feed samples in,
  get armed/broken verdict out): pins slack immunity (no trigger below arm
  threshold), peak tracking, and the drop criterion - host-tested RED-first.
- **TC-003:** break over-travel bound: at max test speed (30 mm/min) and
  worst-case detection latency (2 samples x 100 ms + LP-filter-independence
  argument documented), documented maximum post-break travel; the spec owner
  accepts the bound for sample safety.
- **TC-004:** buffer capacity: 8192 samples x 8 B = 64 KB static buffer; a
  boot/selftest measurement documents minimum free heap AFTER buffer
  allocation (the buffer is static, so the TC guards WiFi/WebServer
  headroom, not the allocation; comparable firmware measures 148-226 KB
  free with WiFi AP up); truncation flag verified host-side.
- **TC-005:** CSV dump integrity: streamed line count equals sample count;
  chunked resume from index N produces lines N..end exactly once. Host-test
  on the pure formatter (no serial in the unit).
- **TC-006:** homing timeout (configurable, default 30 s at seek speed) and
  the minimum-travel instant-trigger guard (FR-001) are pure timing and
  distance decisions (host-tested with injected clock and position).
- **TC-007:** two-stage homing (fast seek -> back-off -> slow re-seek) is a
  pure staged state machine, host-tested: stage transitions, bounce
  immunity of the second touch, and datum identity between stage-2 trigger
  and stage-1 trigger within the re-seek tolerance (TC pinned per #81); a
  stage-2 instant re-trigger (stuck switch) faults per FR-001.

### Non-Functional

- No new libraries (FastAccelStepper + HX711 lib_deps suffice).
- Serial remains 115200 8N1; the dump must not block the motor task (chunked
  writes from the loop task only).
- Web UI unchanged this milestone; the machine keeps serving status over
  WiFi while a test runs (view-only).

## Key Entities & Data Model

- **TestSample:** index (implicit 10 Hz tick), position mm (float), force N
  (float) - 8 B fixed-interval record.
- **TestConfig:** test speed mm/min, arm threshold N, drop fraction pct,
  homing seek speed, homing timeout s - NVS-persisted, clamped.
- **TestRun:** state, sample count, truncation flag, peak force, break
  displacement - one run at a time (RAM), overwritten by the next test.

## Assumptions & Handoffs

- **A1:** load cell identified at the bench: YZC-516C S-type (tension and
  compression), rated 500 kg = 4903 N, sensitivity 2.0 +/-0.05 mV/V
  -> 10 mV full-scale at 5 V excitation (vendor listings; mV/V per
  ANYLOAD FSO guide). Unit of record is Newton. Provisional defaults:
  `testarm 50` (about 1% FS, well above the ~1 N HX711 10 SPS practical
  noise floor), `testdrop 50` (percent of running peak); both NVS-persisted
  and tunable per sample. S-type overload limits (~120-150% FS service)
  noted for the TC-003 over-travel bound review.
- **A2:** endstop hardware is defined: Endstop 1 (bottom, zero datum) on
  GPIO 25, Endstop 2 (upper travel limit) on GPIO 26 - both free of current
  assignments (STEP 18 / DIR 19 / EN 5+23 / HX711 32+33 / rail 34), both
  support internal pull-ups. Switch model: KW4-3Z-3 microswitch (SPDT,
  COM/NO/NC exposed, 5 A/250VAC - logic use only; snap action, KLS7-KW4-3Z
  datasheet). Wiring decision (operator-confirmed, 2026-10-07): **both
  endstops on COM+NC** - idle LOW (closed to GND), pressed = contact opens =
  HIGH (rising-edge trigger, active-high logic). Consequence + guards:
  a broken wire is indistinguishable from a pressed switch, so (a) boot
  selftest treats HIGH on an endstop as WARN ("pressed or broken wire -
  jog off the switch and re-run"), and (b) homing faults if the trigger
  arrives before a minimum travel distance from the start position
  (instant-trigger = broken wire, never a real datum). Wiring task for the
  user: two microswitches to GND; no external resistors needed.
- **A3:** one data set in RAM; nothing persists across reboot (no flash wear,
  no filesystem). A power cycle loses the last run - accepted.
- **A4:** host-side plotting (pyserial/spreadsheet) is out of firmware scope;
  the CSV format is the contract.
- **Next Step:** clarify round complete; `/spec.plan` (data model,
  contracts, task breakdown).

## Success Criteria

- **SC-001:** on the bench, a single `test` command runs home -> pull ->
  break -> stop hands-free; the motor stops within the TC-003 bound after
  the sample breaks.
- **SC-002:** the CSV retrieved over USB plots a force-displacement curve
  with visible elastic region, peak, and break drop.
- **SC-003:** homing seek timeout (endstop missing/stuck, e.g. disconnected
  switch) and power-FAIL injection both abort safely with driver disabled
  and stable fault codes reported; a top-limit trip (D26) stops the pull as
  a guarded stop with the run marked `limit`.
