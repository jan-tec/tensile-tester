# AGENTS.md - tensile-tester

ESP32 firmware (PlatformIO + Arduino) for a tensile test machine: stepper
motor via TMC2209 (13.73:1 gear, 2 mm/rev spindle), HX711 load cell,
WiFi-AP web UI, serial CLI.

## Layout

| Path | Purpose |
|------|---------|
| `src/main.cpp` | Hardware adapter (~1050 lines): firmware + web UI inlined as `INDEX_HTML` PROGMEM string |
| `lib/FirmwareCore/` | Pure C++ gear/feed math (no Arduino) - unit-tested on the host |
| `test/` | Unity host tests (`pio test -e native`) |
| `standalone_tests/` | Debug sketches (blink, raw loadcell, motor menu) - NOT built automatically (see below) |
| `include/` | Empty scaffolding, README only |

GitHub Actions CI (`.github/workflows/ci.yml`) runs two separate jobs on push
to `main` and on every PR: `build` (`pio run -e esp32dev`) and `test`
(`pio test -e native` host unit tests). No linter.

IDE C++ LSP will report `Arduino.h` not found and friends - false positive,
the ESP toolchain headers are only visible to PlatformIO. Ignore LSP
diagnostics in `src/main.cpp`; trust `pio run` output instead.

## Build / flash

`pio` is installed locally via pipx (PlatformIO Core 6.1.19 - same version CI
pins; `~/.local/bin` is in PATH via `~/.bashrc`). **Local build is the primary
verification: run it before pushing any PR** (see Testing below). The first
build downloads the `espressif32` toolchain (~200 MB) into `~/.platformio`;
later builds are fast and offline.

Gotcha: pipx venvs ship without pip, so PlatformIO tool installs can fail
with `MissingPackageManifestError`. Fix once, then rebuild:

```bash
~/.local/share/pipx/venvs/platformio/bin/python -m ensurepip
```

```bash
pio run -e esp32dev            # build the firmware
pio run -t upload -e esp32dev  # flash (needs board on USB port)
pio device monitor -p <port> -b 115200
```

## Testing (full TDD, local-first)

| Layer | Where | What |
|-------|-------|------|
| **Primary (unit)** | Local, host | `pio test -e native` - Unity tests for `lib/FirmwareCore` (pure C++, no Arduino) |
| **Compile gate** | Local | `pio run -e esp32dev` |
| **Integration** | Target | Flash + serial/web smoke test when the board is on the bench: FreeRTOS tasks, portMUX, AccelStepper timing, NVS, HX711 timing, WebServer |
| **Gate** | CI | Separate `test` (host) + `build` (esp32dev) jobs on every push/PR; additional test infra (future test matrix, coverage) lives in CI, not locally |

**TDD rule (all new logic):**

1. Pure logic (gear/feed math, filters, calibration compute, command parsing,
   state machines) goes into `lib/FirmwareCore/` (pure C++: no `Arduino.h`,
   no `PROGMEM`/`F()`, no `String`, no `portMUX`) with the matching test in
   `test/`.
2. **RED:** add the failing test first -> `pio test -e native`.
3. **GREEN:** minimal implementation until `pio test -e native` is green.
4. **REFACTOR:** keep `pio test -e native` AND `pio run -e esp32dev` green.
5. `src/main.cpp` stays the hardware adapter (pins, FreeRTOS tasks, NVS,
   driver I/O, WebServer); hardware-dependent behavior is verified by target
   smoke test, not by host mocks.

**Host vs target boundary:** unit-test state machines and decisions
synchronously on the host; real FreeRTOS tasks, portMUX exclusion,
AccelStepper timing, NVS persistence and HX711 GPIO timing stay
target-tested.

**Why Unity:** `pio test` only supports its bundled runners; Unity is the C++
one. Catch2/GoogleTest would need custom wiring for no gain at this size.

Rule: local unit tests + local build green **before** opening a PR; the CI
`test` and `build` checks green **before** merging.

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
- Gear math (200 steps/rev x 13.73 gear x 8 microsteps = 21968 steps per
  spindle revolution, spindle pitch 2 mm/rev) lives in `lib/FirmwareCore/`,
  unit-tested on the host. Keep it in sync if the hardware changes.
- Loadcell: HX711 on GPIO 32/33, sampled at 10 Hz in the loop task,
  low-pass filtered (alpha 0.18). Calibration factor + valid flag persist in
  NVS Preferences namespace `testmachine`.
- The web UI is the PROGMEM string `INDEX_HTML` in main.cpp. Any UI change is
  a firmware change: rebuild, reflash, then the browser must reload.
- The TMC2209 driver runs plain STEP/DIR/EN via AccelStepper `DRIVER` mode.
  No TMC library is declared in `platformio.ini` (the formerly listed
  TMCStepper/TMC2209 lib_deps were never included and have been removed).
  Do not add TMC UART config or libraries without an actual hardware reason;
  UART is not wired.

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
| Gate | Local `pio test -e native` + `pio run -e esp32dev` green before opening a PR (primary); the PR's CI `test` + `build` checks green before merge (independent second opinion). No automated enforcement exists. |

**Why no hard gate (process-only):** `origin` is a **private** repo on a
**free personal** account (`satwareAG-ironMike`). GitHub blocks both rulesets
and classic branch protection on private repos without a paid plan
(403 "Upgrade to GitHub Pro or make this repository public", verified
2026-08-19). So `main` protection is convention + CI feedback, not a
ruleset. A direct push to `main` is still technically possible - avoid it;
use the PR flow. If a hard gate is ever wanted, options are: upgrade the
account to GitHub Pro, or make the repo public (rulesets are free on public
repos).
