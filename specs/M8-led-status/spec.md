# Feature Specification: M8 - LED Status Language (GPIO2)

Status: Draft | Rigor: Spec-Anchored | Created: 2026-10-10 | Depends on: M1 (DeviceModel), M3 (TestCycleModel, endstop inputs #76) | Issue: #128

**Goal:** The ESP32 DevKit onboard LED (GPIO2) becomes a visual logging
channel: blink patterns encode machine state and hardware events so the
operator reads the machine at a glance from the bench - no serial, no web.
Primary day-one use case: optical confirmation during endstop wiring (#76).

## Research Basis (2026-10-10, first-shot correctness)

| Finding | Source |
|---------|--------|
| Flash safety: <= 3 general flashes/s; risk band 10-25 Hz (peak), 3-30 Hz (broad) | WCAG 2.3.1 "Three Flashes or Below Threshold" (WAI); Epilepsy UK; NIH/PMC "International Guidelines for Photosensitive Epilepsy" (2024) |
| Prioritized severity patterns, first-match-wins; heartbeat = single short flash every ~10 s; double-blink = degraded | OpenShock firmware wiki "Visual Status LED Patterns" (real ESP32 single-LED practice) |
| PWM duty is NOT perceptual brightness: ~20% duty reads as ~50% bright; correct with gamma ~2.2 (`duty = level^2.2`) | jared.geek.nz "Linear LED PWM"; codeinsecurity "The problem with driving LEDs with PWM" |

**Consequence:** the original #128 draft violates flash safety ("very fast
60 ms" = 8.3 Hz; "5 fast" = 4.2 Hz). This spec supersedes those timings.
Design cap: **no pattern exceeds 2 flashes/s** (headroom under the 3/s limit).

## User Scenarios

### US1 (P1) - Read the machine at a glance

As an operator, I recognize the machine state from ~3 m without a screen.

- **Given** the machine in any state, **When** I look at the LED, Then I can
  map the pattern to exactly one state of {SelfTest, Idle, Homing, Running,
  Error} within one pattern repetition (~2 s).

### US2 (P1) - Optical endstop confirmation (bench wiring)

As an operator wiring endstops (#76), I press a switch by hand and see
immediate optical feedback.

- **Given** Idle state, **When** I press Endstop 1 (GPIO 25) or Endstop 2
  (GPIO 26), Then the LED shows the endstop event overlay (1 long flash)
  within 100 ms of the edge, for two repetitions, then returns to Idle
  heartbeat.
- **Given** I release the switch, Then no new overlay (edge-triggered, one
  overlay per press).

### US3 (P1) - Photosensitive-safe by construction

As a safety property, no LED pattern flashes dangerously.

- **Given** any state or event sequence, **When** the LED runs any pattern,
  Then the flash rate never exceeds 3/s (verified as a host-test invariant
  over the whole pattern table, not by eyeball).

### US4 (P2) - Brightness as emphasis, never as data

As a developer, I use PWM intensity to weight attention, not to encode values.

- **Given** Idle heartbeat vs Error pattern, Then the error is also the
  brightest (100%); no pattern encodes a measured value in brightness.

## Requirements

### Functional

- **FR-001 (Single channel):** one LED channel on GPIO2 driven by LEDC PWM.
  Update is non-blocking (no `delay()`); called from the main loop; update
  budget < 1 ms.
- **FR-002 (Layered model):** base layer = machine state; overlay layer =
  transient events. Priority encoder, first-match-wins:
  `Error state > safety event > info event > base state`.
  In Error state overlays are suppressed (FR-008).
- **FR-003 (Timing grid, WCAG-safe):** all patterns are built from:
  flash ON = 150 ms (short) or 600 ms (long); gap inside a group = 350 ms
  (500 ms cycle = 2 flashes/s max); group pause = 1000-1500 ms. No pattern
  may exceed 3 flashes/s - enforced by TC-101 over the pattern table.
- **FR-004 (Pattern table v1):**

  | Signal | Pattern | Duty |
  |--------|---------|------|
  | SelfTest (boot) | 1 long flash, 1.5 s pause, repeat | 100% |
  | Idle | 1 short flash every 3 s | 20% |
  | Homing | 2 short flashes + 1.5 s pause | 60% |
  | Running (test cycle) | steady ON | 40% |
  | Error | 3 short flashes (500 ms cycle) + 1.5 s pause | 100% |
  | Event: endstop edge (D25/D26) | 1 long flash x2 | 100% |
  | Event: break detected | 2 short flashes x2 | 100% |
  | Event: calibration applied | short + long, x2 | 100% |

  Command rejection is deliberately NOT an LED event: the LED is a glance
  channel, serial/web are authoritative for command feedback (review #130).

  Every repeating base pattern is unique in flash count/length within one
  repetition. Overlay/base shape overlaps (endstop 1-long vs SelfTest
  1-long; break 2-short vs Homing 2-short) cannot co-occur: each event has
  an emission window (FR-005) disjoint from the base states of equal shape.
- **FR-005 (Event overlay semantics):** overlays are edge-triggered, play
  2 repetitions (~2-3 s), then the base layer resumes. The latest event
  replaces a running overlay (no queue; serial/web remain the authoritative
  log - the LED is a glance channel). Emission windows: endstop edges fire
  only in Idle/Homing/Running (inputs are monitored from Idle onward);
  break fires only in Running (BreakDetector active); calibration fires
  only in Idle. No overlay originates during SelfTest or Error (FR-008).
- **FR-006 (Brightness):** duty is gamma-corrected: `duty = level^2.2`.
  Four levels only: 20/40/60/100% (duties ~2.9/13.3/32.5/100%). Intensity
  never encodes values (US4).
- **FR-007 (State source):** the LED module owns no state. It consumes the
  existing DeviceModel snapshot (App/Diagnostics) plus M3 cycle state
  (Homing/Running) through a small mapping in `App` - consistent with
  FR-007 contract style of M1.
- **FR-008 (Error suppression):** while the DeviceModel is in Error state,
  the LED shows only the Error pattern; events do not preempt.
- **FR-009 (Strapping-pin safety):** GPIO2 is an ESP32 strapping pin; the
  LED is not configured before boot decisions are made (configured in App
  boot after selftest start; no bootloader interaction).
- **FR-010 (Documentation):** the pattern table ships in the operator docs
  (`docs/led-status.md`) exactly as implemented; any pattern change is a
  doc change in the same PR.

### Constraints & TDD Targets

- Pattern engine is pure C++ in `lib/FirmwareCore` (no Arduino/PROGMEM):
  `PatternTable` (const data), `LedPatternFSM` (virtual-clock step:
  input = state/event/elapsed ms, output = {on/off, duty level}).
- Host tests (Unity, `pio test -e native`): pattern lookup per state;
  overlay preemption; error suppression; virtual-clock walks assert exact
  on/off sequences; **TC-101 flash-rate invariant over ALL patterns**;
  gamma table values pinned.
- `src/LedStatus.{h,cpp}`: thin - LEDC setup + per-loop tick delegating to
  the FSM. Hardware timing verified on target (bench smoke).
- GPIO2 LED presence varies among DevKit clones - verified visually on the
  bench board before M8 tasks start (A1).

### Non-Functional

- Photosafety by construction (US3, TC-101), not by review convention.
- Distinguishable at ~3 m: minimum ON 150 ms, max 2 flashes/s chosen for
  readability over urgency.
- No heap allocation; no task spawns (loop-integrated like LoadCell).

## Key Entities & Data Model

```cpp
enum class LedEvent : uint8_t { None, EndstopEdge, Break, CalApplied };
struct PatternStep { uint16_t onMs; uint8_t level; };   // gap implied by grid
struct Pattern      { const PatternStep* steps; uint8_t count;
                      uint16_t pauseMs; uint8_t repeats; };
// LedPatternFSM: update(state, event, nowMs) -> {bool on, uint8_t duty}
```

## Assumptions & Handoffs

- **A1:** bench DevKit has a GPIO2 LED (classic ESP32-DevKit layout; no LED
  exists on the Mainboard-ATLAS carrier - verified in schematic 2026-10-10).
  Visual check first M8 task.
- **A2:** M3 cycle states (Homing/Running) are reachable through the
  existing model surface; if not, M8 adds a read-only accessor (no logic
  move).
- **A3:** pattern timings may be tuned after bench readability review -
  timing changes keep TC-101 green and update `docs/led-status.md` in the
  same PR.
- **Next Step:** `/spec.plan` -> task breakdown (T001 pure FSM + tests,
  T002 LedStatus driver, T003 event wiring endstop/break/cal,
  T004 docs + bench smoke), one task = one issue = one PR.

## Success Criteria

- **SC-001:** pressing Endstop 1 by hand shows the long-flash overlay
  within 100 ms (bench, #76 wiring session).
- **SC-002:** each of the 5 base states is identifiable from ~3 m within
  one repetition by an unbriefed observer given only the doc table.
- **SC-003:** TC-101 (flash-rate <= 3/s over all patterns) is part of the
  permanent host suite; `pio test -e native` and `pio run -e esp32dev`
  stay green.
- **SC-004:** loop budget: LED update adds < 1 ms per loop iteration
  (asserted by timing check in bench smoke, no oscilloscope required).
