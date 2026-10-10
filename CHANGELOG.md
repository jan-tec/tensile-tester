# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

No release has been tagged yet. The first release is planned as `v0.1.0`
(`0.x` = initial development: hardware firmware with a changing command set
and API).

## [Unreleased]

### Added

- M3 implementation plan + task breakdown (`specs/M3-tensile-test-cycle/`):
  pure `FirmwareCore` contracts for the test-cycle FSM, two-stage homing, break
  detector, sample buffer and CSV formatter, `0x05xx` endstop error block,
  bench matrix B1-B8 (2026-10-10)
- `src/Text.h` message tables (M2-T002, 2026-10-09, #32): `Msg` enum with 56
  ids plus parallel EN/DE columns (EN primary per FR-001, DE as the ASCII
  transcription FR-006 requires for serial), guarded by the TC-002 host test
  and compile-time enum/table sync asserts. Skeleton only - no call site
  routes through it until T003
- Feature specifications M3-M7 (2026-10-07, #75/#77/#78/#84): automated
  tensile test cycle, constant-force hold, load cell calibration wizard
  with physics-window guard, structured JSON serial API, web UI redesign
  (draft)
- `Locale` resolution in `FirmwareCore` (M2-T001, 2026-10-07, #74):
  locale parse + string-table index with EN fallback, host-tested RED-first
- FastAccelStepper (RMT) motion (2026-10-07, #71): hardware-timed steps,
  3 MB app partition (`huge_app.csv`); second driver enable line: EN2
  (motor 2) on GPIO23, switched together with EN1 (motor 1, GPIO5) on
  enable/disable and at boot

### Changed

- Native test env pins gnu++17 (2026-10-07, #73): AppleClang defaults to
  C++98 and rejected `constexpr`/`enum class`; explicit flag makes host
  tests compiler-independent
- Project language switched to English-primary (2026-10-07, M2 FR-008):
  docs, specs, and changelog in English; UI text becomes locale-table
  driven (M2 in progress)

- M1 startup selftest + health diagnostics (2026-08-21, #20-#29): boot
  selftest of 5 checks (BOARD, NVS, POWER, HX711, MOTOR) with stable
  machine codes in `src/ErrorCode.h` and the `BOOT`/`STATE`/`CHECK` log
  grammar (FR-002 to FR-007); device state machine
  (Booting -> SelfTest -> Idle/Error) host-tested in `lib/FirmwareCore/`;
  5 V rail sense on GPIO34 (boot POWER check + 1 Hz runtime monitor with
  hysteresis and 5-sample debounce, FR-005); build/verify/interpret
  reference in `docs/hardware-power-check.md` (TC-003(a) no-wiring
  matrix captured, bench reaches Idle at 1.04 s, SC-001)
- Error state behavior (FR-009, #28): any selftest FAIL -> Error state:
  driver disabled, motion commands rejected on serial and Web with code
  + message; recovery by reset/reboot only
- Selftest presentation (FR-008, #29): `GET /api/status` carries
  `state`, `faultCode` and `selftest{overall,checks[]}` with stable
  machine names; new "Selbsttest" card in the web UI; `overallStatus()`
  in `src/ErrorCode.h` (host-tested, 60/60)
- M0 module split (2026-08-19 to 21, #8-#17): `src/` is modular: `App`
  (orchestration), `Commands` (shared command policy), `SerialCli` (serial
  input loop), `WebUi` (softAP + routes), `Motion` (stepper task), `LoadCell`
  (HX711 sampling + calibration), `Settings` (NVS), `UiHtml.h` (PROGMEM UI
  asset); `main.cpp` reduced from ~1050 lines to a 31-line composition root
- `IMotion`/`ILoadCell` contracts (FR-007): `App` is reference-injected via
  the interfaces; zero dynamic allocation verified (no `new`/`delete`)
  (2026-08-21, #17)
- Command parser in `lib/FirmwareCore/` (host-tested): `commandFromName()`
  + `parseCommandValue()`; three legacy dispatch quirks pinned in host tests
  and tracked under the No-Quirk Policy: #41 (case-sensitive command names),
  #42 (no word boundary in prefix dispatch), #43 (value parse accepts
  garbage / truncates) (2026-08-21, #9)
- Host-side unit test foundation for full TDD: `lib/FirmwareCore/`
  (platform-independent gear/feed math, extracted from `src/main.cpp`),
  `test/test_gear_math.cpp` (10 Unity tests, `env:native`), and a separate CI
  `test` job running `pio test -e native` next to the `build` job (2026-08-20,
  #5)
- Initial PlatformIO project for ESP32 (`esp32dev`, Arduino framework)
  (2026-04-12, `283123b`, `5ac554a`)
- Debug sketches in `standalone_tests/`: LED blink, HX711 raw measurements
  (2026-04-12, `34d006e`)
- Load cell (HX711, GPIO 32/33) with serial menu for calibration and force
  measurement (2026-04-12, `5c1db96`)
- Stepper control over serial CLI (`h`, `en`, `off`, `zero`, `pos`, `rpm`,
  `acc`, `move`, `turn`, `sweep`, `stop`; relative units in degrees at this
  point) with gear math: 200 steps/rev, 13.73:1 gearbox, 8 microsteps; two
  NEMA 17 motors driven synchronously via STEP/DIR/EN (2026-04-13, `7e0e344`)
- WiFi AP mode (`ESP32-Testmaschine`) with web UI: live force plot, status
  panel, feed/acceleration forms, and `GET /api/status` / `GET /api/command`
  endpoints; load cell now sampled at 10 Hz with low-pass filtering;
  mm-based `move`/`turn` and `feed` commands with 2 mm/rev spindle pitch
  (2026-04-13, `75c07e7`)
- Load cell calibration from the web UI (`cal_zero` + `cal_apply <grams>`)
  with factor persisted in NVS (Preferences namespace `testmachine`) and
  reloaded at boot; jog buttons and `jog up` / `jog down` commands (relative
  1000 mm travel) (2026-04-14, `f7ce4b4`)

### Changed

- Stepper pulse generation moved from AccelStepper (software polling task on
  core 0 + `portMUX`) to FastAccelStepper, which drives the ESP32 RMT
  peripheral in hardware: step timing is no longer jittered by WiFi/scheduler
  preemption, so motion is smooth and the usable step rate no longer caps at
  the poll-loop rate. `Motion` no longer owns a FreeRTOS task or `portMUX`;
  `IMotion`, the command set, and the gear math are unchanged. FastAccelStepper
  is pinned to the commit after 1.4.0 that fixes the ESP32 build (2026-10-05).
- `GET /api/status`: `forceNewton` is now the string `"n/a"` when the
  load cell is not ready, instead of a zero reading (SC-003, #24); the
  web UI shows `n/a` in the force panel for the same case
- Behavior-preserving refactor of the monolithic `src/main.cpp` into the M0
  modules above (T002-T010): serial command set, Web API, boot log, and UI
  byte-identical; each module verified by host tests + target smoke (serial
  full command set, Web curl + browser render) (2026-08-21, #8)

### Removed

- Dead `TestMode` scaffolding (enum, `activeTestMode` state, always-Manuell
  "Modus" pill, disabled "Tests" select) and the `testMode` field from
  `GET /api/status` (2026-08-20, #3)
- Unused TMC2209 library dependencies (`teemuatlut/TMCStepper`,
  `janelia-arduino/TMC2209`) from `lib_deps` - the driver runs plain
  STEP/DIR/EN via AccelStepper (2026-08-20, #3)

### Fixed

- Boot no longer logs `E (NNN) gpio: gpio_pulldown_en(123): GPIO number error
  (input-only pad has no internal PD)`. `PowerSense::begin()` requested
  `INPUT_PULLDOWN` on GPIO34; GPIO34-39 are input-only pads without internal
  pull-ups/pull-downs, so the IDF call failed and applied nothing. Now plain
  `INPUT`, and `docs/hardware-power-check.md` states the consequence: the
  defined low level that makes `POWER = NOT_WIRED` deterministic has to come
  from the external divider R2 (#27), it was never provided by the MCU.
  Target-verified 2026-10-10 (boot log after re-flash)

### Fixed

- One dropped `/api/status` response no longer leaves `Status kann nicht geladen
  werden.` pinned in the message bar while the status pills keep updating every
  100 ms. The message is now cleared on the next successful poll, so a transient
  AP hiccup cannot masquerade as a persistent connection failure (reproduced on
  the bench: a single injected failed poll kept the red message on screen for
  minutes although polling had recovered)

### Known limitations

- Automatic test modes ("Ziehen bis Bruch", "Kraft halten") are not
  implemented; the scaffolding for them was removed in #3 and will be
  re-added in the PR that implements the first mode.
