# Tensile Tester

[![Ask DeepWiki](https://deepwiki.com/badge.svg)](https://deepwiki.com/jan-tec/tensile-tester)

ESP32 firmware for a tensile test machine (Zugversuchsmaschine): a
gear-stepper spindle with load-cell force measurement, an automated pull-to-
break test cycle with CSV data capture, a constant-force hold mode, and a
WiFi web UI plus serial CLI. English-first with a German locale switch
(`lang de` or the Web UI selector; persisted in NVS).

## Hardware

| Component | Model / value |
|-----------|---------------|
| MCU | ESP32 (PlatformIO board `esp32dev`) |
| Stepper driver | TMC2209, plain STEP/DIR/EN (shared STEP/DIR, EN1/EN2 per driver; no UART configuration wired) |
| Stepper motor | NEMA 17, 200 full steps/rev, x2 (run synchronously on the same signals) |
| Gearbox | 13.73:1 |
| Spindle pitch | 2 mm/rev |
| Load cell | HX711 ADC (YZC-516C) |

### Wiring

| Signal | GPIO | Notes |
|--------|------|-------|
| STEP | 18 | |
| DIR | 19 | |
| EN1 | 5 | motor 1 driver, active low (LOW = driver enabled) |
| EN2 | 23 | motor 2 driver, active low, switched together with EN1 |
| HX711 DOUT | 32 | |
| HX711 SCK | 33 | |
| Endstop 1 (bottom, datum) | 25 | `INPUT_PULLUP`, NC wiring (broken wire reads as pressed) |
| Endstop 2 (top, travel limit) | 26 | `INPUT_PULLUP`, NC wiring |
| Status LED | 2 | onboard LED, LEDC PWM, configured after boot selftest |
| 5 V rail sense | 34 | 10 k/10 k divider - **not built yet**, POWER check reads `NOT_WIRED` |

Mainboard CAD sources: `hardware/mainboard-atlas/` (KiCad 9).

## Requirements

- PlatformIO CLI: `pipx install platformio` (first build downloads the
  `espressif32` toolchain, ~200 MB)
- ESP32 dev board on USB for flashing

## Build and flash

```bash
pio run -e esp32dev            # build (only env)
pio run -t upload -e esp32dev  # flash
pio device monitor -p <port> -b 115200
```

Boot runs a selftest (BOARD, NVS, POWER, HX711, MOTOR, ENDSTOP1, ENDSTOP2)
and logs `BOOT`/`STATE`/`CHECK` lines; any FAIL locks the device in the
Error state with the driver off (recovery = reboot).

## Serial CLI (115200 baud)

Same command policy as the web API (`Commands::execute()`); unknown commands
print help. Replies are localized text; a JSON envelope (M6,
`lib/FirmwareCore/src/ApiResponse.h`) wraps replies and runner events for
host automation.

| Command | Effect |
|---------|--------|
| `en` / `off` | enable / disable both driver outputs |
| `zero` | set axis position to 0 |
| `tare` | tare the load cell |
| `pos` | print position (steps + mm) and force |
| `force` | print current force in N |
| `stop` | stop motion; aborts a running homing/test cycle first |
| `rpm <U/min>` | set gear output speed |
| `acc <U/min/s>` | set acceleration |
| `feed <mm/min>` | set spindle feed rate (clamped 0.5 - 60) |
| `move <mm>` | relative spindle move |
| `turn <revs>` | relative gear output rotation |
| `jog up` / `jog down` | continuous jog for a relative 1000 mm travel; stop with `stop` |
| `cal_zero` | calibration step 1 (load cell unloaded) |
| `cal_apply <grams>` | calibration step 2 with a known mass in grams |
| `home` / `home top` | staged homing to the bottom datum / move to the top limit |
| `test` | run the tensile test cycle (pull until break, capture samples) |
| `testspeed <mm/min>` | test/homing speed (persisted) |
| `testarm <N>` | break-detector arm threshold (persisted) |
| `testdrop <%>` | break criterion: force drop from peak (persisted) |
| `homeseek <mm/min>` | homing seek speed (0 = follow testspeed; persisted) |
| `hometimeout <s>` | homing seek timeout, 1-300 s (persisted) |
| `chold <N> [minutes]` | constant-force hold at N newtons, optional duration |
| `choltol <%>` | hold tolerance band in % of setpoint (persisted) |
| `choldclear <mm>` | hold approach clearance (persisted) |
| `choldmax <N>` | per-sample hold force ceiling, 1-4000 N (persisted) |
| `data [from-index]` | stream captured samples as CSV (resumable) |
| `testdata` | one-line sample/peak summary |
| `ends` | bench telemetry: endstops, homing phase, cycle state, hold phase |
| `lang <en\|de>` | UI locale (persisted) |

## Web UI

The ESP32 starts in WiFi **AP mode** (no client network needed):

- SSID: `ESP32-Testmaschine`, password: `testmaschine`
- UI at the softAP IP (default `192.168.4.1`; the actual IP is printed to
  serial and reported in `/api/status`)

The M7 UI is a monochrome, single-red-accent operator surface with one
global sticky Stop. The tear workflow is first-class: Home -> sample
prompt -> Start test -> run summary -> force-displacement curve -> CSV
download. It also carries the boot-selftest card, manual motion/calibration
controls, the test settings forms (testspeed/testarm/testdrop) and the
EN/DE selector. Hold visualization is view-only pending M4-T007.

API (all endpoints are GET):

| Endpoint | Payload |
|----------|---------|
| `/api/status` | JSON: driver/motion state, position/steps, force, calibration, `state` + `faultCode` + `selftest{overall,checks[]}`, cycle/homing/hold state, `network` |
| `/api/command?cmd=<command>` | executes a CLI command; returns `{"ok":bool,"message":"..."}` |
| `/api/data` | the run CSV, same grammar as the serial `data` dump; rejected while a cycle is active or a serial dump is in flight |

## Load cell calibration

1. Remove the load, run step 1 (`cal_zero` / "1. Unbelastet bestaetigen").
2. Hang a known mass, enter the mass in grams, run step 2
   (`cal_apply <grams>` / "2. Gewicht messen").

The calibration factor persists in NVS (Preferences namespace `testmachine`)
and is loaded automatically at the next boot. Force is displayed in newtons
(converted from grams with g = 9.80665 m/s^2). Which mass to use and why:
`docs/calibration.md` (calibrate near the working load).

## Motion math

| Parameter | Value |
|---------|-------|
| Motor full steps/rev | 200 |
| Gear ratio | 13.73 |
| Microsteps | 8 |
| Steps per spindle revolution | 21968 |
| Spindle pitch | 2 mm/rev |
| Feed clamp | 0.5 - 60 mm/min |

The constants live in `lib/FirmwareCore` (host-tested) - keep them in sync
if the gearbox, motor or spindle changes.

## Repository layout

| Path | Purpose |
|------|---------|
| `src/` | Firmware modules: composition root (`main.cpp`), `App` orchestration, `Commands`, `SerialCli`, `WebUi` + `UiHtml.h`, `Motion`, `LoadCell`, `Settings`, `Diagnostics`, `Endstops`, `PowerSense`, `TestRunner`, `LedStatus`, `Text` |
| `lib/FirmwareCore/` | Pure C++ (no Arduino): gear/feed math, command parser, state machines, homing/test/hold models, break detector, sample buffer + CSV, LED patterns, JSON envelope |
| `test/` | Unity host tests (`pio test -e native`) |
| `specs/` | Milestone specs, plans, task backlogs (`specs/README.md` is the roadmap) |
| `docs/` | Operator docs: calibration, LED patterns, power-rail check |
| `hardware/` | KiCad mainboard sources |
| `standalone_tests/` | Debug sketches - not built automatically |

### Running a standalone test

PlatformIO compiles everything in `src/`, so a debug sketch is NOT built
automatically. Swap it into `src/main.cpp` temporarily and never commit the
swapped state (details in AGENTS.md).

## Status

| Feature | State |
|---------|-------|
| Manual control (web UI + serial CLI) | done |
| Load cell measurement + persisted calibration | done |
| Boot selftest + Error state, i18n DE/EN, LED status language | done |
| Staged homing to a datum | done (bench-verified) |
| Automated pull-to-break test cycle + CSV capture | done (bench-verified) |
| Constant-force hold (`chold`, closed-loop) | firmware done; formal bench evidence open (M4-T008/T009) |
| Hold web view + buffer keep-last ring | open (M4-T007/T004) |
| Guided calibration wizard (M5) | open (manual `cal_zero`/`cal_apply` work) |
| 5 V rail divider hardware | open (POWER reads `NOT_WIRED`) |

## History and versioning

See [CHANGELOG.md](CHANGELOG.md). The project follows Semantic Versioning;
no release has been tagged yet (see `[Unreleased]`).
