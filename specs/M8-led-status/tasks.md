# Tasks: M8 - LED Status Language (GPIO2)

Plan: `plan.md`. Gate per AGENTS.md: local `pio test -e native` +
`pio run -e esp32dev` green before a PR; CI `test` + `build` green before
merge. **1 task = 1 issue = 1 PR.** TDD: RED first for pure tasks.

**Operator priority: earliest flashable firmware with endstop->LED
feedback.** Order below is the ASAP path: T001 (pure) -> M3-T008
(pulled forward, D1) -> T002 (driver + wiring) -> **flash + bench B1-B3**
-> T003/T004.

## Pure logic (`lib/FirmwareCore`, host-tested)

- [x] T001 [US1] [US3] [US4] [FR-003] [FR-004] [FR-005] [FR-006] [FR-008] [TC-101]
  `LedPattern` in `lib/FirmwareCore/src/LedPattern.h`: `LedBase`/`LedEvent`/
  `Level` enums, `LedPatternFSM` (base patterns, overlay 2 reps, latest-wins,
  Error suppression), `duty255()` gamma table (7/34/83/255), pattern table per
  spec FR-004. RED: `test/test_led_pattern.cpp` (all 5 base patterns walked on
  a virtual clock, overlay start/preempt/expire, Error drops overlay, duty
  table, **TC-101 flash-rate invariant over every pattern**)
- [x] T003 [US1] [FR-004] [FR-005] break + cal events wiring (M3-T011 runner
  feeds `Break`; `Commands` feeds `CalApplied` on `cal_apply` success; Homing/
  Running bases live from the cycle model). Not before M3-T011. Bench evidence
  deferred to M3-T013/T014 sessions

## Target modules (`src/`, hardware glue, target-tested)

- [x] T002 [US1] [US2] [US3] [FR-001] [FR-002] [FR-007] [FR-009] [SC-001]
  [SC-002] [SC-004] `src/LedStatus.{h,cpp}` + `src/App.{h,cpp}` wiring:
  LEDC ch0 1 kHz 8-bit on GPIO2 (configured after selftest start, D7),
  `tick()` from `App::loop()` (order after `endstops_.poll()`), App maps
  `DeviceModel`/cycle state -> `LedBase` (D2) and gates events to emission
  windows (D4). **First bench step: confirm the GPIO2 LED exists (A1).**
  Includes M3-T008 `Endstops` consumption: edge -> `EndstopEdge` from
  Idle/Homing/Running. Bench B1-B3 (SC-001: press D25 -> long flash x2
  within 100 ms), `pos` cadence unchanged (SC-004 proxy). Flash + verify
  on the board the same session

## Pulled forward from M3 (own issue+PR, M3 spec ownership)

- [x] M3-T008 [US1] [US3] [M3 FR-001] [M3 FR-007] `src/Endstops.{h,cpp}`:
  GPIO 25/26 `INPUT_PULLUP`, `poll()` with 3-consecutive-sample debounce,
  `bottom()`/`top()` level getters, `bottomEdge()`/`topEdge()` rising-edge
  flags cleared on read, `bootState()` for the M3-T013 selftest rows.
  Compile gate + bench read-back (D1; M3 plan T008 text unchanged)

## Docs & close

- [ ] T004 [US1] [FR-010] `docs/led-status.md` with the exact pattern table
  as implemented; README pointer. Close M8 milestone with SC-001..SC-004
  evidenced (SC-002 unbriefed-observer pass can follow async, does not
  block the bench session)

## Dependencies

```
T001 (pure FSM)   M3-T008 (Endstops, pulled forward)
      \              /
       v            v
         T002 (LedStatus + App wiring + endstop overlay)
                |
                v
         FLASH + BENCH B1-B3   <- operator's ASAP milestone
                |
                v
   T003 (break/cal events)  --blocked on M3-T011
   T004 (docs + close)      --after T002 patterns are bench-confirmed
```
