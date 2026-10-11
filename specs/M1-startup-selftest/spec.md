# Feature Specification: M1 - Startup Selftest & Health Diagnostics

Status: In progress (T008 #27 hardware divider + T011 #30 target verify open) | Rigor: Spec-Anchored | Created: 2026-08-20 | Depends on: M0

**Goal:** On startup the device determines which peripherals are connected
and healthy (load cell, power supply, motor driver, board) and reports
stable machine-readable error codes plus clear human messages (serial + Web
UI + boot log). Designed for the current bench state - **ESP32 on USB only,
no GPIO wiring**: missing peripherals are warnings, not failures.

## User Scenarios

### US1 (P1) - Boot report

As an operator, I want the device to tell me at boot which peripherals are
present and healthy, so I know what a test run can and cannot do.

- Given no load cell wired, When the device boots, Then it reports
  `HX711 NOT_CONNECTED` (warning) and still reaches `Idle` (motion
  operable, force shown as `n/a`).
- Given a load cell wired, When the device boots, Then it reports
  `HX711 OK` and force measurement is available.

### US2 (P1) - Clear errors

As an operator, I want every fault to carry a code and a human-readable
message on serial and Web UI, so I can fix it without a logic analyzer.

- Given power below the failure threshold, When the check runs, Then the
  device enters `Error` with `0x0201 POWER_LOW` and the message
  "Versorgungsspannung zu niedrig" (M1 is German; M2 switches to English).

### US3 (P2) - Parseable logs

As a maintainer, I want a stable log grammar for headless diagnosis.

- Given any boot, When I read serial at 115200, Then I see
  `BOOT fw=<ver> git=<sha8> reset=<reason>`, one
  `CHECK id=<id> result=<status> [code=0xNNNN meas=<v>] elapsed_ms=<n>` per
  check, and `STATE old=<s> new=<s> [code=0xNNNN]` per transition.

## Requirements

### Functional

- **FR-001:** Device state machine `Booting -> SelfTest -> Idle | Error` in
  `lib/FirmwareCore` as a pure `DeviceModel` (no `Arduino.h`); every fault
  path disables the motor driver.
- **FR-002:** Check set: `BOARD` (reset reason, free heap, flash size),
  `NVS` (open + calibration config sane), `POWER` (5 V rail via ADC),
  `HX711` (timeout + signal plausibility + variance), `MOTOR` (EN toggle;
  no sense wiring exists -> report `NO_FEEDBACK`, honest).
- **FR-003:** Result shape: `{id, status, errorCode, measurement,
  elapsedMs}` with `status in {OK, WARN, FAIL, NOT_CONNECTED, NOT_WIRED}`;
  stable machine codes `0x01xx` NVS, `0x02xx` power, `0x03xx` HX711,
  `0x04xx` motor.
- **FR-004:** Absence is not a fault: missing HX711 -> `WARN` +
  `NOT_CONNECTED` (force `n/a`, motion operable); power sense not wired ->
  `NOT_WIRED` (info).
- **FR-005:** Power check: resistor divider 5 V rail -> **GPIO34** (10 k /
  10 k, hardware task T008); thresholds FAIL < 4.50 V, WARN < 4.75 V;
  runtime monitoring at 1 Hz with hysteresis and >= 5 consecutive
  out-of-band samples before `FAIL`; until wired: `NOT_WIRED`.
- **FR-006:** HX711 detection: `wait_ready` with 1000 ms timeout ->
  `NOT_CONNECTED`; otherwise plausibility + 500 ms variance window -> `OK`
  (or diagnostic `WARN NO_VARIATION`).
- **FR-007:** Log grammar per US3, **always English** (logs are
  language-neutral; user-facing messages localize in M2).
- **FR-008:** Presentation: `/api/status` gains a `selftest` object
  (`overall` + `checks[]`); serial prints the boot report; Web UI shows a
  selftest panel (German ASCII labels per current convention).
- **FR-009:** `Error` state behavior: driver disabled, motion commands
  rejected with code + message; recovery by reset/reboot (-> `SelfTest`).
- **FR-010:** `docs/hardware-power-check.md`: divider schematic, pin,
  thresholds, wiring.

### Constraints & TDD Targets

- **TC-001:** `DeviceModel`: 100% of transitions + fault paths host-tested,
  RED-first - `test/test_device_model.cpp`.
- **TC-002:** `HealthEval` pure helpers (threshold evaluation, hysteresis,
  variance) host-tested RED-first - `test/test_health_eval.cpp`.
- **TC-003:** Target verification matrix: (a) current bench, no wiring ->
  expected report table; (b) HX711 wired -> `HX711 OK`; (c) full wiring ->
  all `OK`.

### Non-Functional

- Selftest completes <= 5 s (HX711 1 s timeout dominates).
- No new PlatformIO libraries; boot-time checks do not disturb the loop
  rate; runtime power check at 1 Hz only.

## Success Criteria

- **SC-001:** Boot on the current bench (power divider not wired) reaches
  `Idle` within 5 s with warnings only: `POWER NOT_WIRED`, and
  `HX711 NOT_CONNECTED` (load cell absent) or
  `HX711 WARN HX711_NO_VARIATION 0x301` (load cell present, no load
  change in the 500 ms variance window). Verified 2026-08-21: `Idle` at
  t=1.040 s (`docs/hardware-power-check.md`, TC-003(a)).
- **SC-002:** Every `FAIL` carries code + human message visible on serial
  and Web UI.
- **SC-003:** Motion fully operable with the load cell absent (force
  `n/a` in UI + `/api/status`).

## Assumptions / Limitations

- TMC2209 is wired STEP/DIR/EN only (no status pin, no UART): motor
  faults are **not directly detectable**; `MOTOR NO_FEEDBACK` is the honest
  report. A limit switch or encoder would be follow-up hardware.
- 5 V rail sensing needs the divider (T008); `VSYS` (GPIO36) is not used -
  it reflects the 3.3 V domain, not the rail under load.

## Next Step

Execute `specs/M1-startup-selftest/tasks.md` in dependency order (after M0
is merged).
