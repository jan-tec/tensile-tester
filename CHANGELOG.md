# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

No release has been tagged yet. The first release is planned as `v0.1.0`
(`0.x` = initial development: hardware firmware with a changing command set
and API).

## [Unreleased]

### Added

- Constant-force hold, M4 core (2026-10-11, #205-#213 stream): `HoldConfig`
  fields in `TestConfig` (tolerance/clearance/max, NVS round-trip, clamped),
  pure `HoldController` FSM (approach -> holding band, lost-rule with 2-tick
  confirm, time/stop/limit/lost/fault end reasons, mean/maxDev/inBand/creep
  accounting), `TestRunner` Hold mode reusing the homing stages,
  `chold <N> [min]` / `choltol` / `choldclear` / `choldmax` commands,
  `hold_summary` runner event, `choldmax` per-sample ceiling + approach-lost
  rule after the #220 probe-loss incident, permanent HOP telemetry in
  `ends` (#221/#222). First bench holds passed (SC-001-style: 18.5-18.6 N
  steady at a 20 N setpoint, in-band 94.2%). Formal SC evidence open
  (M4-T008/T009). Spec: `specs/M4-constant-force-hold/`
- Web UI redesign, M7 (2026-10-11, #172/#173): monochrome single-red-accent
  operator surface, one global sticky Stop, first-class tear workflow
  (Home -> sample prompt -> Start test -> run summary -> force-displacement
  curve -> CSV download via `GET /api/data`), test settings forms, EN/DE
  dictionary (web DE with native umlauts), Selbsttest card; run summary
  peak survives a no-op config write
- Serial JSON API, M6 (2026-10-11, #185-#194): locale-neutral JSON envelope
  for serial replies and runner events (`lib/FirmwareCore/src/ApiResponse.h`,
  one state snapshot per line, `help`/`pos` data fragments); the web HTTP
  body stays the plain message. Spec: `specs/M6-serial-json-api/`
- M3 target wiring + bench close-out (2026-10-10/11, #133/#142/#144/#148/
  #151/#153/#162/#163/#165/#166/#219): `Endstops` selftest rows (7 checks),
  `TestRunner` homing/pull/tail orchestration, `home`/`home top`/`test` +
  config/dump commands, DIR polarity fix (`dirHighCountsUp=false` - `move +5`
  is physically UP), `homeseek`/`hometimeout` knobs, staged-homing datum
  achieved on the bench (`Travel -0.001 mm`) after the same-tick
  forceStop/re-command race fix, switch-repeatability allowance in the
  homing trigger guards, full cycle B1 evidence (155.71 N @ 11.67 mm break,
  598 samples, clean CSV + resumable dump)
- `docs/calibration.md` (2026-10-11, #82 stream): weight-sizing guidance
  (calibrate near the working load, ~80% of expected max; mass accuracy
  dominates; no traceability claim). Bench calibration 2026-10-11: 2465 g
  reference mass, factor 8.923326 counts/g, two-sided verification ~0.3%

- LED status language (M8, #128/#132/#134/#136, 2026-10-10): `lib/FirmwareCore/src/LedPattern.h`
  pure pattern engine (5 base patterns as functions of time, 2-rep event
  overlays latest-wins, Error phase-restart + overlay drop, gamma-2.2 duties
  7/34/83/255) with the TC-101 flash-safety invariant (<=3 rising edges per
  1000 ms - WCAG 2.3.1 headroom - asserted over every pattern, 13 host
  tests); `src/LedStatus.{h,cpp}` LEDC driver on GPIO2 (1 kHz, 8-bit,
  write-on-change, configured post-selftest - strapping pin); App wiring
  maps DeviceState -> base pattern and endstop edges (GPIO 25/26) -> the
  long-flash overlay outside Error. Bench-verified: heartbeat, SC-001
  endstop press feedback (#134 evidence). Spec + research record:
  `specs/M8-led-status/` (WCAG 2.3.1, NIH/PMC photosensitivity guidance,
  OpenShock pattern practice, gamma PWM sources); operator doc
  `docs/led-status.md`
- M3 T008-T010 hardware primitives (2026-10-10, #133/#142/#144):
  `src/Endstops.{h,cpp}` GPIO 25/26 `INPUT_PULLUP`, 5 ms cadence,
  3-sample debounce, rising edges cleared on read, bootState snapshot;
  `IMotion::startContinuous(direction)` (runForward/runBackward homing
  seeks) + `forceStop()` (immediate stop, queue flushed - `stop()` keeps
  its ramp); `ILoadCell::rawForceNewton()` pre-filter snapshot of the
  poll() sample (BreakDetector input, no second blocking HX711 read)
- M3 T003-T007 pure models (2026-10-10, #125/#126/#127/#123/#124):
  `TestCycleModel` transition table (incl. `Limit` no-code, `Aborted`,
  `BufferFull`), `SampleBuffer` + exact-once `formatCsvChunk()` CSV
  formatter, `TestConfig` defaults + `clampTestConfig()` bounds, `0x05xx`
  endstop error block (0x0501-0x0504, no renumbering), parser for
  `home`/`home top`/`test` + `data <from>` index. All host-tested RED-first
  (suite 96 -> 170 cases)
- M2 T003-T008 locale completion (2026-10-10, #107/#109/#110/#111/#112/#113/#114):
  serial CLI routes through `msg()` with `lang` command; M1 fault
  presentation localized; English-first web UI with embedded EN/DE
  dictionary; locale persists across reload (server authority); locale
  drift gate host test; English-first project docs
- Hardware CAD sources (2026-10-10, #129): `hardware/mainboard-atlas/`
  KiCad 9 project (schematic, PCB, symbols/footprints, gerbers) imported
  from the designer's untracked `raw/`; ENDSTOP1 = GPIO25 on J5 verified
  via kicad-cli netlist export; `*.kicad_prl` untracked (kicad-cli
  rewrites it)

- Locale system end to end (M2 T003-T006, #33/#34/#35/#36): English-first
  UI. Serial CLI strings route through `Text::msg(Msg)` (fresh boot speaks
  English), `lang <en|de>` + NVS `locale` key persist the choice; M1 fault
  codes localize via `msgForFault` on serial and in a new `/api/status`
  `faultMessage` field; the Web UI serves English statics plus an embedded
  EN/DE JS dictionary with a header selector and follows the server locale
  (`locale` field); the client keeps the last locale across reloads
  (localStorage) while the server stays the authority. Bench-verified TC-003
  matrix + flash budget 2026-10-10 (#37): EN+DE strings 3607 B, RAM +8 B
- `HomingModel` (`lib/FirmwareCore/src/Homing.h`, M3 T002, #108): pure
  staged homing FSM (BackOff -> SeekDown -> FinalSeek; SeekUp -> BackOffDown
  for `home top`, which never sets a datum), injected time + debounced
  endstop booleans, clearance-consumed guard, stuck-switch guard, per-phase
  seek timeout. Host-tested RED-first (19 cases); not yet wired into `src/`
- Locale drift gate (M2, #89, #113): host test rejects DE-copied-from-EN
  outside an explicit locale-invariant allowlist;
  `scripts/check_locale_coverage.py` (CI `test` job) flags stale `Msg` rows
  and raw user-facing literals in `Serial.print`/`jsonMessage` call sites
- `src/Text.h` single-source message table (M2 #88, #107): enum + EN/DE
  columns generated from ONE `MSG_X_LIST` X-macro, pairing enforced by
  construction (GCC cannot use C++20 designated initializers on arrays)
- `BreakDetector` (`lib/FirmwareCore/src/BreakDetect.h`, M3 T001, #104): pure
  arm/peak/drop break detector - arms strictly above the arm threshold, tracks
  the running peak only while armed, latches `Broken` below
  `dropFraction x peak`, ignores NaN/inf samples. No Arduino, no heap,
  host-tested RED-first (13 cases). Not yet wired into `src/` (T011).
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

- `CHECK`/`STATE` boot-log lines print machine codes with the FR-007 fixed
  width: `code=0x0301`, not `code=0x301`. `Serial.print(v, HEX)` drops the
  leading zeros and shifted the columns fixed-width host log parsers key on.
  New pure `errorCodeHex()` in `src/ErrorCode.h` is the single padding point
  (2026-10-10, #97, host-tested)
- Boot no longer logs `E (NNN) gpio: gpio_pulldown_en(123): GPIO number error
  (input-only pad has no internal PD)`. `PowerSense::begin()` requested
  `INPUT_PULLDOWN` on GPIO34; GPIO34-39 are input-only pads without internal
  pull-ups/pull-downs, so the IDF call failed and applied nothing. Now plain
  `INPUT`, and `docs/hardware-power-check.md` states the consequence: the
  defined low level that makes `POWER = NOT_WIRED` deterministic has to come
  from the external divider R2 (#27), it was never provided by the MCU.
  Target-verified 2026-10-10 (boot log after re-flash)

- One dropped `/api/status` response no longer leaves `Status kann nicht geladen
  werden.` pinned in the message bar while the status pills keep updating every
  100 ms. The message is now cleared on the next successful poll, so a transient
  AP hiccup cannot masquerade as a persistent connection failure (reproduced on
  the bench: a single injected failed poll kept the red message on screen for
  minutes although polling had recovered)

- Jog buttons are reachable from the keyboard and carry text names. They were
  wired to `pointerdown`/`pointerup` only, so Enter/Space on a focused jog button
  did nothing (measured on the bench: two presses produced zero `/api/command`
  requests) - WCAG 2.1.1. `keydown`/`keyup` now mirror press-and-hold (auto-
  repeat ignored, Space no longer scrolls the page) and `blur` stops the jog the
  way `pointerleave` does, so a key released after focus leaves cannot leave the
  machine running. The buttons are a bare `U+25B2`/`U+25BC` glyph, so they also
  got `aria-label` (WCAG 4.1.2), and `#message` is now
  `aria-live="polite"` so command replies and connection errors are announced
  (WCAG 4.1.3). Three new host cases in `test/test_ui_html.cpp` went red first

- Web UI no longer 404s on `/favicon.ico` (#59): `src/UiHtml.h` declares an
  inline SVG data-URI icon, so the browser stops requesting a file the firmware
  never served and the console error on every page load is gone. `UiHtml.h` is
  now host-includable (Arduino-guarded `PROGMEM`) and `test/test_ui_html.cpp`
  is the first automated gate on the served HTML asset (negative control run:
  removing the icon link turns the test red). Target-verified 2026-10-10 with
  the board on the bench

### Known limitations

- Hold-mode formal bench evidence open (M4-T008/T009): needs a surviving
  test sample; two samples broke below rating (grip stick-slip suspected).
- Web hold view and the sample-buffer keep-last ring are open (M4-T007/T004,
  jan-tec #14/#13); the buffer currently stops capture at 8192 samples and
  flags `truncated`.
- Guided calibration wizard (`calibrate <grams>`, M5 FR-001) not
  implemented; the manual `cal_zero`/`cal_apply` path is the working
  surface (single cal point; linearity above the 2.465 kg reference mass
  unverified, jan-tec #7).
- 5 V rail divider on GPIO34 not built (jan-tec #5): the POWER check reads
  `NOT_WIRED` and B6 of the M3 bench matrix stays open.
- Web UI JS has no automated bench test (jan-tec #9); the M7 redesign
  invalidated the recorded pre-redesign manual baseline.
- Serial/web `feed` clamp stays 0.5-60 mm/min; the bench hard max is the
  owner directive 12 mm/min (operator discipline, not enforced in firmware).
