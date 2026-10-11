# Implementation Plan: M0 - Module Split

## 1. Technical Context

ESP32 (Arduino-ESP32 core, PlatformIO 6.1.19), FreeRTOS, AccelStepper
(`DRIVER` mode), HX711, WebServer, Preferences (NVS). Host tests: Unity via
`pio test -e native`. **No new dependencies.**

## 2. Target Layout

Vertical decomposition (2026 research consensus for existing firmware,
section 7): keep the framework, extract behavior into modules, `main.cpp`
as composition root ("intentionally boring").

| File | Responsibility | Est. lines |
|------|----------------|-----------|
| `src/main.cpp` | Composition root: create objects, wire interfaces, `setup()`/`loop()` | ~80 |
| `src/App.{h,cpp}` | Orchestration: loop (status print, 10 Hz force tick, dispatch), holds `IMotion&`/`ILoadCell&` | ~120 |
| `src/Motion.{h,cpp}` | AccelStepper, motor task (core 0), `portMUX`, enable/disable, `startMoveTo`, position/running, rpm/acc settings | ~200 |
| `src/LoadCell.{h,cpp}` | HX711 init, 10 Hz sampling, low-pass (0.18), tare, calibration compute | ~150 |
| `src/Settings.{h,cpp}` | NVS `Preferences` namespace `testmachine`: calibration factor + valid flag (later: locale) | ~60 |
| `src/Commands.{h,cpp}` | `executeCommand` policy, command list, `String` adapter to FirmwareCore parser | ~230 |
| `src/SerialCli.{h,cpp}` | Serial 115200 input loop | ~50 |
| `src/WebUi.{h,cpp}` | `WebServer`, `INDEX_HTML` (`PROGMEM`), `/`, `/api/status`, `/api/command`, `statusJson` | ~280 |
| `lib/FirmwareCore/` | Pure: gear math (existing) + `CommandParser` (new, T002) | +60 |

Dependency direction (no cycles):
`main.cpp -> App -> {Commands, SerialCli, WebUi} / {Motion, LoadCell, Settings} -> lib/FirmwareCore`.

## 3. Interfaces (FR-007)

```cpp
// src/IMotion.h
struct IMotion {
  virtual ~IMotion() = default;
  virtual void begin() = 0;
  virtual void enable() = 0;
  virtual void disable() = 0;
  virtual void setOutputRpm(float rpm) = 0;
  virtual void setAccelRpmPerS(float a) = 0;
  virtual void startMoveMm(float mm) = 0;
  virtual void stop() = 0;
  virtual bool isRunning() const = 0;
  virtual float positionMm() const = 0;
};

// src/ILoadCell.h
struct ILoadCell {
  virtual ~ILoadCell() = default;
  virtual void begin() = 0;
  virtual float readForceNewton() = 0;
  virtual void tare() = 0;
  virtual void setCalibrationFactor(float f) = 0;
  virtual bool isReady() const = 0;
};
```

Objects created statically in `main.cpp`, references injected into `App`.
No `new`/`delete` anywhere (embedded standard, section 7).

## 4. TDD Mapping

| Task | Phase | RED artifact | GREEN gate |
|------|-------|--------------|-----------|
| T002 | New logic | `test/test_command_parser.cpp` failing first | `pio test -e native` |
| T003-T009 | REFACTOR | - (existing suite stays green) | host test + `pio run` + target smoke |
| T010 | REFACTOR | - | host test + `pio run` + target smoke |

Coverage: TC-003 (FirmwareCore >= 90%) via gear tests (existing) + parser
tests (T002).

## 5. Key Decisions

- **D1 - Refactor, not rewrite.** Module-by-module extraction, each PR
  independently verifiable (research consensus: vertical decomposition
  beats big-bang rewrites for existing firmware).
- **D2 - `.cpp` modules, not header-only.** Every module has non-trivial
  state; header-only C++ slows the build and hides implementation.
- **D3 - `String` stays in the app layer; `char*` boundary in FirmwareCore.**
  `String` is fine for presentation/adapter code; the pure core stays
  allocation-free (research: keep pure logic testable, String at the edges).
- **D4 - Keep the two control paths (serial + Web) sharing one
  `Commands::execute`.** Existing architecture, unchanged.
- **D5 - Keep Arduino + PlatformIO.** Mature libraries (AccelStepper,
  HX711, WebServer), moderate-timing firmware; Arduino-ESP32 is itself an
  ESP-IDF component, so no capability is lost (research section 6).
- **D6 - One module per PR, <= 3 files per step** (Baby Steps).

## 6. Risks

| Risk | Mitigation |
|------|-----------|
| `PROGMEM` `INDEX_HTML` relocation | Stays a `PROGMEM` array in `WebUi.cpp`; no copy, same size |
| Global `stepper`/`motionMux` encapsulation | Move into `Motion` internals; never expose the raw `AccelStepper` |
| Refactor regression on target | Per-task gate: host test + build + target smoke (serial + Web spot-check) |
| Two motors on one STEP/DIR/EN | Unchanged: `Motion` keeps the single-driver synchronous model |

## 7. Research Citations (2026-08-20, Perplexity, high context)

| Topic | Source |
|-------|--------|
| PlatformIO `src/` vs `lib/` layout | docs.platformio.org: project structure (src = app, lib = libraries) |
| Arduino-ESP32 as ESP-IDF component (hybrid path) | docs.espressif.com: Arduino-ESP32 library |
| Event-driven state machine pattern | blog.mbedded.ninja: design patterns, state machines |
| Embedded dependency injection (narrow interfaces, ctor injection, no heap) | embeddedrelated.com; Stack Overflow 51540157 |
| NVS `Preferences` usage | docs.espressif.com: Preferences tutorial |
| ESP32 watchdog/reset-reason context | docs.espressif.com: ESP-IDF monitoring (wdt, reset reasons) |
