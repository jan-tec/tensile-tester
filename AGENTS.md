# AGENTS.md - tensile-tester

ESP32 firmware (PlatformIO + Arduino) for a tensile test machine: stepper
motor via TMC2209 (13.73:1 gear, 2 mm/rev spindle), HX711 load cell,
WiFi-AP web UI, serial CLI.

## Layout

| Path | Purpose |
|------|---------|
| `src/main.cpp` | The only compiled file (~1100 lines): ALL firmware + web UI inlined as `INDEX_HTML` PROGMEM string |
| `standalone_tests/` | Debug sketches (blink, raw loadcell, motor menu) - NOT built automatically (see below) |
| `test/`, `include/`, `lib/` | Empty scaffolding, README only |

GitHub Actions CI (`.github/workflows/ci.yml`) builds `pio run -e esp32dev`
on push to `main` and on every PR; the check context is `build`. No linter,
no test runner.

IDE C++ LSP will report `Arduino.h` not found and friends - false positive,
the ESP toolchain headers are only visible to PlatformIO. Ignore LSP
diagnostics in `src/main.cpp`; trust `pio run` output instead.

## Build / flash

PlatformIO CLI is NOT preinstalled on this workstation (`pio` not in PATH,
verified 2026-08-19). Install first: `pipx install platformio`. The first
build downloads the `espressif32` toolchain (~200 MB).

```bash
pio run -e esp32dev            # build (only env)
pio run -t upload -e esp32dev  # flash (needs board on USB port)
pio device monitor -p <port> -b 115200
```

## Running a standalone test (gotcha)

PlatformIO compiles everything in `src/`; `src/main.cpp` is currently the
only file there, so a debug sketch is NOT built automatically. To flash one,
swap it into `src/` temporarily - and NEVER commit the swapped state:

```bash
mv src/main.cpp /tmp/main.cpp.bak
cp standalone_tests/test_02_loadcell_measure_raw.cpp src/main.cpp
pio run -e esp32dev -t upload
mv /tmp/main.cpp.bak src/main.cpp
```

## Architecture (read before editing main.cpp)

- **WiFi AP mode only**, hardcoded SSID `ESP32-Testmaschine` / `testmaschine`.
  Web UI at the softAP IP (default 192.168.4.1; exact IP printed to serial and
  in `/api/status` field `network`).
- **Two control paths share `executeCommand()`**: serial CLI (115200) and
  Web API `GET /api/command?cmd=<string>` (yes, commands are GET). Add a
  command once, both paths get it. Commands: `en` `off` `zero` `tare` `pos`
  `force` `stop` `rpm <U/min>` `acc <U/min/s>` `feed <mm/min>` `move <mm>`
  `turn <revs>` `jog up|down` `cal_zero` `cal_apply <grams>`.
- **Stepper runs in a dedicated FreeRTOS task on core 0**; every stepper
  access is wrapped in `portMUX` critical sections (`motionMux`). Never block
  inside them. Two physical motors are driven synchronously from the same
  STEP/DIR/EN signals.
- Gear math constants are at the top of main.cpp: 200 steps/rev x 13.73 gear
  x 8 microsteps = 21968 steps per spindle revolution, spindle pitch 2 mm/rev.
  Keep them in sync if the hardware changes.
- Loadcell: HX711 on GPIO 32/33, sampled at 10 Hz in the loop task,
  low-pass filtered (alpha 0.18). Calibration factor + valid flag persist in
  NVS Preferences namespace `testmachine`.
- The web UI is the PROGMEM string `INDEX_HTML` in main.cpp. Any UI change is
  a firmware change: rebuild, reflash, then the browser must reload.
- TMC2209 driver libraries are declared in `platformio.ini` (both
  teemuatlut/TMCStepper and janelia-arduino/TMC2209, marked "ODER") but
  **neither is included anywhere** - the driver is plain STEP/DIR/EN via
  AccelStepper `DRIVER` mode. Do not add TMC UART config without an actual
  hardware reason; it is currently not wired.

## Conventions

- German text in firmware and UI is **ASCII-transcribed** (ae/oe/ue/ss, e.g.
  `Waegezelle`, `gueltige`). Keep source ASCII-only - do not introduce
  umlauts into .cpp/.ini.
- Remotes: `origin` = satwareAG-ironMike/tensile-tester (mirror),
  `upstream` = jan-tec/tensile-tester (single branch `main`).

## Branching & sync model (agreed 2026-08-19)

| Rule | Detail |
|------|--------|
| `upstream` | Read-only reference. `git fetch upstream` + inspect only. Never `git push` there. |
| `origin/main` | Trunk, protected **by convention only** (process-only gate). All changes land here via PR. |
| No permanent dev branch | Trunk-based; `main` is always shippable. |
| Change flow | Short-lived branch (`fix/...`, `feat/...`, `docs/...`) -> PR -> squash merge into `main`. |
| Upstream sync | `git fetch upstream` -> create `sync/upstream-YYYYMMDD` -> `git merge upstream/main` there -> PR -> squash/merge. **Never merge upstream directly into `main`.** |
| CI gate | The PR's `build` check must be green before merge. No automated enforcement exists. |

**Why no hard gate (process-only):** `origin` is a **private** repo on a
**free personal** account (`satwareAG-ironMike`). GitHub blocks both rulesets
and classic branch protection on private repos without a paid plan
(403 "Upgrade to GitHub Pro or make this repository public", verified
2026-08-19). So `main` protection is convention + CI feedback, not a
ruleset. A direct push to `main` is still technically possible - avoid it;
use the PR flow. If a hard gate is ever wanted, options are: upgrade the
account to GitHub Pro, or make the repo public (rulesets are free on public
repos).
