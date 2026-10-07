# AGENTS.md - tensile-tester

ESP32 firmware (PlatformIO + Arduino) for a tensile test machine: stepper
motor via TMC2209 (13.73:1 gear, 2 mm/rev spindle), HX711 load cell,
WiFi-AP web UI, serial CLI.

## Layout

| Path | Purpose |
|------|---------|
| `src/main.cpp` | Composition root (31 lines): global module wiring + `setup()`/`loop()` delegation to App |
| `src/App.{h,cpp}` | Orchestration: boot sequence (Serial, Motion, banner, LoadCell, Diagnostics selftest, Web) + main loop + driver-off on Error (FR-009) |
| `src/Commands.{h,cpp}` | Command policy: `execute()` shared by serial and Web; FirmwareCore parser via `String` adapter |
| `src/SerialCli.{h,cpp}` | Serial 115200 input loop (line buffer, CR skip, LF dispatch, help on unknown) |
| `src/WebUi.{h,cpp}` | WebServer, softAP, routes `/`, `/api/status`, `/api/command`, `statusJson` |
| `src/UiHtml.h` | Pure data: the PROGMEM `INDEX_HTML` asset (no logic, SC-001) |
| `src/Motion.{h,cpp}` | FastAccelStepper (RMT hardware-timed steps), enable/disable, moves, rpm/accel |
| `src/LoadCell.{h,cpp}` | HX711 10 Hz sampling + low-pass (0.18), tare, two-step calibration |
| `src/Settings.{h,cpp}` | NVS Preferences namespace `testmachine`: calibration factor + valid flag |
| `src/Diagnostics.{h,cpp}` | Boot selftest (SelfTest state): 5 checks BOARD/NVS/POWER/HX711/MOTOR, `BOOT`/`STATE`/`CHECK` log grammar (FR-007), owns the DeviceModel |
| `src/PowerSense.{h,cpp}` | 5 V rail sense: GPIO34 10 k/10 k divider, one-shot boot read + 1 Hz runtime monitor (FR-005) |
| `src/ErrorCode.h` | Stable machine codes + check vocabulary (pure, host-testable) - never renumber codes |
| `src/IMotion.h`, `src/ILoadCell.h` | Contracts (FR-007): App depends on them, Motion/LoadCell implement them |
| `lib/FirmwareCore/` | Pure C++ gear/feed math, command parser, device state machine, health evaluation (no Arduino) - unit-tested on the host |
| `test/` | Unity host tests (`pio test -e native`) |
| `standalone_tests/` | Debug sketches (blink, raw loadcell, motor menu) - NOT built automatically (see below) |
| `include/` | Empty scaffolding, README only |

GitHub Actions CI (`.github/workflows/ci.yml`) runs two separate jobs on push
to `main` and on every PR: `build` (`pio run -e esp32dev`) and `test`
(`pio test -e native` host unit tests). No linter.

IDE C++ LSP will report `Arduino.h` not found and friends - false positive,
the ESP toolchain headers are only visible to PlatformIO. Ignore LSP
diagnostics in `src/*.cpp`; trust `pio run` output instead.

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

Partition scheme: `huge_app.csv` (`board_build.partitions` in `platformio.ini`)
gives a 3 MB app partition instead of the 1.25 MB default, which
FastAccelStepper would otherwise overflow. The firmware uses no OTA and no SPIFFS,
so neither is lost. NVS stays at `0x9000`, so a re-flash keeps the stored
calibration. If OTA is ever needed, `min_spiffs.csv` gives 1.875 MB x2.

## Testing (full TDD, local-first)

| Layer | Where | What |
|-------|-------|------|
| **Primary (unit)** | Local, host | `pio test -e native` - Unity tests for `lib/FirmwareCore` (pure C++, no Arduino) |
| **Compile gate** | Local | `pio run -e esp32dev` |
| **Integration** | Target | Flash + serial/web smoke test when the board is on the bench: FastAccelStepper RMT timing, NVS, HX711 timing, WebServer |
| **Gate** | CI | Separate `test` (host) + `build` (esp32dev) jobs on every push/PR; additional test infra (future test matrix, coverage) lives in CI, not locally |

**TDD rule (all new logic):**

1. Pure logic (gear/feed math, filters, calibration compute, command parsing,
   state machines) goes into `lib/FirmwareCore/` (pure C++: no `Arduino.h`,
   no `PROGMEM`/`F()`, no `String`, no `portMUX`) with the matching test in
   `test/`.
2. **RED:** add the failing test first -> `pio test -e native`.
3. **GREEN:** minimal implementation until `pio test -e native` is green.
4. **REFACTOR:** keep `pio test -e native` AND `pio run -e esp32dev` green.
5. Hardware-dependent code stays in the `src/` modules (Motion: pins +
   RMT stepper driver, LoadCell: driver I/O, Settings: NVS, WebUi: WebServer);
   hardware-dependent behavior is verified by target smoke test, not by
   host mocks.

**Host vs target boundary:** unit-test state machines and decisions
synchronously on the host; FastAccelStepper RMT timing, NVS persistence and
HX711 GPIO timing stay target-tested.

**Why Unity:** `pio test` only supports its bundled runners; Unity is the C++
one. Catch2/GoogleTest would need custom wiring for no gain at this size.

Rule: local unit tests + local build green **before** opening a PR; the CI
`test` and `build` checks green **before** merging.

## Running a standalone test (gotcha)

PlatformIO compiles everything in `src/`, so a debug sketch is NOT built
automatically. To flash one, swap it into `src/main.cpp` temporarily - and
NEVER commit the swapped state. The module `.cpp` files stay and compile
alongside (they are self-contained TUs and reference nothing defined in
main.cpp), so the image grows a bit - harmless for a bench sketch. If a
sketch collides with a module symbol, move the offending module file aside
too:

```bash
mv src/main.cpp /tmp/main.cpp.bak
cp standalone_tests/test_02_loadcell_measure_raw.cpp src/main.cpp
pio run -e esp32dev -t upload
mv /tmp/main.cpp.bak src/main.cpp
```

## Architecture (read before editing src/)

- **Module structure (M0, 2026-08):** `src/` is modular (see Layout).
  `main.cpp` is a composition root; `App` orchestrates; `Commands::execute()`
  is the single command policy. `App` depends on the `IMotion`/`ILoadCell`
  contracts (FR-007). No dynamic allocation anywhere (no `new`/`delete`).
- **Boot selftest + Error state (M1, 2026-08):** boot runs 5 checks in the
  SelfTest state (BOARD, NVS, POWER, HX711, MOTOR) logging `BOOT`/`STATE`/
  `CHECK` lines; stable machine codes live in `src/ErrorCode.h` (never
  renumber). Any FAIL -> Error state: driver disabled, motion commands
  rejected on serial + Web with code + message (FR-009); recovery is
  reset/reboot only. `/api/status` carries `state`, `faultCode`,
  `selftest{overall,checks[]}`; the web "Selbsttest" card renders them.
  Rail-sense hardware (10 k/10 k divider on GPIO34) + reading the POWER
  check: `docs/hardware-power-check.md`.
- **WiFi AP mode only**, hardcoded SSID `ESP32-Testmaschine` / `testmaschine`.
  Web UI at the softAP IP (default 192.168.4.1; exact IP printed to serial and
  in `/api/status` field `network`).
- **Two control paths share `Commands::execute()`**: serial CLI (115200,
  input loop in `SerialCli`) and Web API `GET /api/command?cmd=<string>`
  (yes, commands are GET, routes in `WebUi`). Add a command once, both paths
  get it. Commands: `en` `off` `zero` `tare` `pos` `force` `stop` `rpm <U/min>`
  `acc <U/min/s>` `feed <mm/min>` `move <mm>` `turn <revs>` `jog up|down`
  `cal_zero` `cal_apply <grams>`.
- **Stepper steps are generated by the ESP32 RMT peripheral** via
  `FastAccelStepper` (hardware-timed, so WiFi/scheduler preemption does not
  jitter the pulse train); the library runs its own stepper task - `Motion` no
  longer owns a FreeRTOS task or `portMUX`. Two physical motors are driven
  synchronously from the same STEP/DIR signals; each driver has its own
  active-low EN line (EN1 GPIO5, EN2 GPIO23), always switched together in
  `Motion::enable()/disable()`.
- Gear math (200 steps/rev x 13.73 gear x 8 microsteps = 21968 steps per
  spindle revolution, spindle pitch 2 mm/rev) and the command parser live in
  `lib/FirmwareCore/`, unit-tested on the host. Keep the gear constants in
  sync if the hardware changes.
- Loadcell (`LoadCell`): HX711 on GPIO 32/33, sampled at 10 Hz in the loop
  task, low-pass filtered (alpha 0.18). Calibration factor + valid flag
  persist in NVS Preferences namespace `testmachine` (`Settings`).
- The web UI is the PROGMEM string `INDEX_HTML` in `src/UiHtml.h` (served by
  `WebUi`). Any UI change is a firmware change: rebuild, reflash, then the
  browser must reload.
- The TMC2209 driver runs plain STEP/DIR/EN, pulsed by FastAccelStepper's RMT
  backend. AccelStepper is no longer a `lib_deps` entry; the
  `standalone_tests/test_03` motor-menu sketch needs `waspinator/AccelStepper`
  re-added if it is flashed. No TMC library is declared in `platformio.ini`
  (the formerly listed TMCStepper/TMC2209 lib_deps were never included and have
  been removed). Do not add TMC UART config or libraries without an actual
  hardware reason; UART is not wired.

## Conventions

- UI text is locale-table driven (`specs/M2-i18n-de-en/spec.md`): serial
  DE output uses ASCII transcription (ae/oe/ue/ss), web DE uses native
  umlauts (UTF-8). Keep source ASCII-only - do not introduce umlauts into
  .cpp/.ini.
- Remotes: `origin` = satwareAG-ironMike/tensile-tester (mirror),
  `upstream` = jan-tec/tensile-tester (single branch `main`).
- **`gh` guardrail**: this workstation's shell exports
  `GITHUB_PERSONAL_ACCESS_TOKEN`, which `gh` prefers over the stored OAuth
  login and resolves repo context to the `jan-tec` account (verified
  2026-08-20: a PR was accidentally created in `jan-tec/tensile-tester` and
  closed immediately). Always pass
  `--repo satwareAG-ironMike/tensile-tester` (or set `GH_REPO`) on every `gh`
  command in this repo.

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

## No-Quirk Policy (NQP)

**Only a fixed quirk is a good quirk.**

A *quirk* is an accidental behavior in existing code discovered during
implementation (e.g. a missing word boundary in command dispatch). Every
quirk found MUST:

| Step | Requirement |
|------|-------------|
| 1. Pin | Freeze the behavior in a host test (`test/`) so refactors cannot drift it silently |
| 2. Track | File a GitHub issue: title `[NQP] ...`, label `quirk`, milestone `Quirks`, with repro, pin location, proposed fix, and acceptance criteria (the issue IS the fix spec) |
| 3. Fix | Only via an explicit behavior-change PR that references the issue and updates the pin in the same PR |

Quirk fixes are behavior changes: they must NOT ride along inside
behavior-preserving refactors (e.g. M0 FR-004). One quirk per PR.

First batch (found in M0-T002, PR #40): #41 (case-sensitive command names),
#42 (no word boundary in prefix dispatch), #43 (garbage-accepting /
truncating value parse).
