# Implementation Plan: M8 - LED Status Language (GPIO2)

Spec: `spec.md` (PR #130, merged). Prior landings this depends on: M1
(`DeviceModel`, `Diagnostics`), M3 pure models (`TestCycleModel` #126,
`HomingModel` #108), M3-T008 `Endstops` (pulled forward, see D1),
#76 endstop wiring, #128.

**Operator priority (2026-10-10): earliest flashable firmware that shows
an endstop press on the LED.** Task order and PR slicing follow that.

## 1. Technical Context

- Loop-integrated modules (no task spawns): `LoadCell` polls at 10 Hz from
  `App::loop()`; `PowerSense` monitors at 1 Hz. The LED driver joins the
  same family: a `tick()` from `App::loop()`, zero blocking.
- Pure logic lives in `lib/FirmwareCore` (host-tested, Unity): gear math,
  parsers, models (`BreakDetector`, `HomingModel`, `TestCycleModel`,
  `SampleBuffer`). The pattern engine joins this family.
- GPIO2: ESP32 strapping pin; DevKit onboard LED (no LED on the carrier,
  schematic verified 2026-10-10). LEDC PWM, 8-bit duty is ample (4 levels).
- Endstop inputs do not exist yet: M3-T008 `src/Endstops` (GPIO 25/26,
  `INPUT_PULLUP`, COM+NC = pressed HIGH, 3-sample debounce) is still open.

## 2. Architecture

```
Endstops (M3-T008) ──edge──┐
BreakDetector (M3 runner) ─┤
Commands (cal applied) ────┤   App maps sources -> LedEvent + LedBase
DeviceModel/TestCycle ─────┘   (FR-007: LED owns no state)
        │
        v
lib/FirmwareCore/src/LedPattern.h   (pure: LedPatternFSM + PatternTable)
        │  Output {on, Level}
        v
src/LedStatus.{h,cpp}   (LEDC on GPIO2, tick() from App::loop, <1 ms)
```

## 3. Key Decisions

- **D1 (M3-T008 pulled forward):** the operator wants endstop->LED feedback
  on the bench ASAP. M3-T008 (`src/Endstops`) is independent in the M3
  dependency graph (T008 <- nothing, T011 needs it) and is the LED's input
  source, so it is implemented now as its own M3 PR; M3-T011 consumes it
  later unchanged. No M3 spec change: T008 is exactly the M3 plan's task.
- **D2 (base-state enum owned by the LED module):** `LedBase {SelfTest,
  Idle, Homing, Running, Error}` is declared in `LedPattern.h`; `App` maps
  `DeviceModel` + `TestCycleModel` states onto it (spec FR-007). The FSM
  never imports model headers - mapping stays in `App` where both models
  are visible.
- **D3 (pattern data model):** `Pattern = steps[] + gapMs(350 fixed grid)
  + pauseMs + repeats`, plus `steady` form for Running. A step is
  `{onMs, Level}`. Idle period 3.0 s = 150 on + 350 gap + 2500 pause.
  Levels are percents (20/40/60/100); duty conversion lives in the same
  header: `duty255(level) = round(255 * (pct/100)^2.2)` -> 7/34/83/255
  (gamma per spec FR-006; values pinned by test, not recomputed).
- **D4 (emission windows in App, FSM stays dumb):** `LedPatternFSM`
  accepts any base/event pair; the App wiring only forwards endstop edges
  from Idle/Homing/Running, break from Running, cal from Idle (spec
  FR-005). Rationale: the FSM stays a pure pattern player (host-testable
  without model knowledge); windows are 6 lines of gating in App.
- **D5 (Error suppression in the FSM, not App):** base == Error forces the
  Error pattern and drops any overlay (spec FR-008). One rule, enforced
  once, testable on the host.
- **D6 (overlay timing):** an event overlay plays exactly 2 repetitions of
  its pattern (~1.5-2.5 s), then the base resumes; a new event restarts
  the overlay. No queue (spec FR-005).
- **D7 (LEDC setup):** channel 0, 1 kHz (above visible flicker fusion, far
  below any bus concern), 8-bit resolution. `ledcWrite` only on
  output change (cached last value).

## 4. Data Model

```cpp
enum class LedBase  : uint8_t { SelfTest, Idle, Homing, Running, Error };
enum class LedEvent : uint8_t { None, EndstopEdge, Break, CalApplied };
enum class Level    : uint8_t { Dim = 20, Mid = 40, High = 60, Full = 100 };

struct LedStep   { uint16_t onMs; Level level; };
struct LedOutput { bool on; Level level; };          // -> LedStatus

class LedPatternFSM {
public:
  void begin(uint32_t nowMs);
  LedOutput update(LedBase base, LedEvent event, uint32_t nowMs);
};
uint8_t duty255(Level level);   // gamma 2.2, pinned by test
```

## 5. Contracts (what T001 pins RED-first)

| Contract | Pinned behavior |
|----------|-----------------|
| `update(base, None, t)` | base pattern as a pure function of elapsed time; Idle period exactly 3000 ms; SelfTest 1 long/2100 ms; Homing 2 short; Error 3 short/3000 ms; Running steady Mid |
| `update(Error, any event, t)` | Error pattern only, overlay dropped (D5) |
| overlay | event != None starts overlay at that timestamp, plays 2 reps, base resumes; a second event mid-overlay restarts (D6) |
| `duty255` | 20->7, 40->34, 60->83, 100->255 (D3 gamma) |
| **TC-101** | for every pattern (base + overlay), no 1000 ms window contains > 3 ON-rising edges (virtual clock, 50 ms sampling) |

## 6. TDD mapping

| Test file | Covers |
|-----------|--------|
| `test/test_led_pattern.cpp` (RED first, T001) | §5 contracts incl. TC-101 |
| Bench (T002, target) | LED visible on GPIO2; endstop press -> long flash x2 within 100 ms (SC-001); state patterns eyeballed (SC-002); loop budget (SC-004) |

## 7. Bench verification (T004, after #76-wired switch on J5)

| ID | Check |
|----|-------|
| B1 | Boot: SelfTest long-flash visible; Idle heartbeat dim |
| B2 | Press Endstop 1 (D25) by hand in Idle -> 1 long flash x2, back to heartbeat; release -> nothing (edge) |
| B3 | Same on Endstop 2 (D26) |
| B4 | `rpm` command -> Homing? no: homing needs M3-T011; use Error injection (disconnect HX711? no - flash-only check) -> skip B4, covered by M3-T013 |
| B5 | Loop budget: serial `pos` cadence unchanged with LED ticking (SC-004 proxy) |

B4 note: Homing/Running base patterns cannot be bench-driven until
M3-T011 lands; their LED rows are verified then (deferred evidence, not
dropped - recorded on the T002 issue).

## 8. Cross-milestone dependencies

```
M3-T008 Endstops (pulled forward, D1) ──> M8-T002 (endstop overlay wiring)
M3-T011 TestRunner (later) ─────────────> M8-T003 (break event + Homing/Running bases live)
M2 Text (landed) ────────────────────────> nothing (LED is language-free)
```

## 9. Risks

| Risk | Mitigation |
|------|-----------|
| DevKit clone without GPIO2 LED | A1: visual check is T002's first bench step; fallback = external LED on a free GPIO, one constant |
| Overlay/base shape overlap misread | emission windows (spec FR-005, D4); bench B2/B3 |
| Pattern tweak requests after bench review | A3: timing changes ride with TC-101 green + docs update in same PR |

## 10. Prior research

Recorded in `spec.md` "Research Basis" (WCAG 2.3.1, NIH/PMC 2024,
Epilepsy UK, OpenShock wiki, gamma-2.2 PWM sources) - not duplicated here.
