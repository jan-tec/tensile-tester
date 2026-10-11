# Tasks: M3 - Automated Tensile Test Cycle

Plan: `plan.md`. Gate per AGENTS.md: local `pio test -e native` +
`pio run -e esp32dev` green before a PR; CI `test` + `build` green before merge.
**1 task = 1 PR.** TDD: RED first for every pure task (T001-T007).

Hardware note: T008-T014 touch GPIO 25/26 - the wiring task **#76** (user)
blocks *verification* (T013/T014), not the code tasks.

## Pure logic (`lib/FirmwareCore`, host-tested)

- [x] T001 [US3] [FR-003] [TC-002] `BreakDetector` in `lib/FirmwareCore/src/BreakDetect.h`: arm threshold, peak tracking only while armed, drop criterion, latching `Broken`, NaN/inf ignored. RED: `test/test_break_detect.cpp` (slack immunity below threshold, no false break, peak tracking, drop verdict, latch)
- [x] T002 [US1] [US3] [FR-001] [FR-008] [TC-006] [TC-007] `HomingModel` in `lib/FirmwareCore/src/Homing.h`: BackOff -> SeekDown -> FinalSeek staged FSM, clearance-consumed instant-trigger guard, stuck-switch guard on the re-seek, seek timeout on injected elapsed time, `startFromTop()` for a D26 hit during back-off, datum identity within `oneStepMm`. RED: `test/test_homing_model.cpp` (incl. `SeekUp`/`BackOffDown` for `home top` and the rule that a top seek never sets a datum)
- [x] T003 [US1] [FR-002] [TC-001] `TestCycleModel` in `lib/FirmwareCore/src/TestCycle.h`: transition table of plan §5 incl. `Limit` (no code), `Aborted`, `Error`, `BufferFull` -> Tail, invalid-transition no-op, `active()` for exclusivity. RED: `test/test_cycle_model.cpp`
- [x] T004 [US2] [FR-004] [FR-005] [TC-004] [TC-005] `TestData.h`: `TestSample` (8 B), `SampleBuffer` (CAPACITY 8192, `truncated`), pure `formatCsvChunk()` with `.` decimals + `#` header/END marker and exact-once chunk resume, pure `testdata` status formatter. RED: `test/test_sample_data.cpp` (line count == sample count, resume from N, tail chunk, empty buffer, truncated flag, **chunk that would split a line emits nothing and does not advance** - TC-005 exact-once depends on it)
- [x] T005 [US1] [FR-005] `TestConfig.h`: defaults per spec A1/A2 + `clampTestConfig()` (feed range, arm (0, 4903], drop [10, 90], clearance [0.5, 10], timeout [1, 300] s, non-finite -> field default). RED: `test/test_config.cpp` (`homingSeekMmPerMin` 0 -> follows `testSpeedMmPerMin`)
- [x] T006 [US3] [FR-007] `src/ErrorCode.h`: append the `0x05xx` endstop block - `EndstopSeekTimeout` 0x0501, `EndstopInstantTrigger` 0x0502, `EndstopStuckSwitch` 0x0503, `EndstopPressedOrBroken` 0x0504 (WARN only) + `errorCodeName()` rows. No renumbering. RED: extend `test/test_error_code.cpp`
- [x] T007 [US1] [US2] [FR-005] parser: `Command::{Home, HomeTop, Test}`, `parseCommandIndex()` for `data <from>`, `isMotionCommand()` extended to `home`/`home top`/`test` (M1 FR-009 rejection). RED: extend `test/test_command_parser.cpp`. `commandFromName()` is an exact `strcmp` chain, so pin both spellings resolving correctly (`home` -> `Home`, `home top` -> `HomeTop`, `hometop` -> `Unknown`) instead of assuming an ordering rule; pin that `test <junk>` is rejected by the exact match and is not swallowed by the `testspeed`/`testarm`/`testdrop` value parsers (named dispatch runs first, `src/Commands.cpp:58`); reject `data -1`/`data abc`; keep the NQP-pinned quirks untouched

## Target modules (`src/`, hardware glue, target-tested)

- [ ] T008 [US1] [US3] [FR-001] [FR-007] `src/Endstops.{h,cpp}`: GPIO 25/26 `INPUT_PULLUP`, `poll()` debounce (D4: 3 consecutive equal samples), `bottom()`/`top()`, `bootState()` for the selftest. Verified by B3/B4 once #76 is wired
- [ ] T009 [US1] [FR-001] [FR-002] `src/IMotion.h` + `src/Motion.{h,cpp}`: append `startContinuous(int direction)` and `forceStop()` (D5: `FastAccelStepper::forceStop()`); existing `stop()` semantics unchanged. Compile gate + bench check that `stop` still decelerates
- [ ] T010 [US3] [FR-003] `src/LoadCell.{h,cpp}` + `src/ILoadCell.h`: `rawForceNewton()` returning the pre-filter snapshot taken inside `poll()` (D6). Do **not** reuse `rawSample()` for the detector - it issues a second blocking `scale_.read()` and desyncs the 10 Hz cadence. Filtered `forceNewton()` output unchanged
- [x] T011 [US1] [US2] [US3] [FR-002] [FR-004] [FR-006] [FR-007] `src/TestRunner.{h,cpp}`: drives homing/pull/tail over `IMotion` + `ILoadCell` + `Endstops`, owns `SampleBuffer`, ticks from `App::loop()`, `forceStop()` on break/limit, D26 hard backstop -> `Limit` (no code), `DeviceModel` Error/PowerFail -> cycle Error with data retained, datum = `IMotion::zero()` at the stage-2 trigger, no offset arithmetic, plus the `homed()` and `dumpInProgress()` accessors (FR-006, D14). Wire into `src/App.{h,cpp}` (tick order after `loadcell_.poll()`)
- [x] T012 [US1] [US2] [FR-005] `src/Commands.{h,cpp}`: handlers for `home`, `home top`, `test`, `testspeed <mm/min>`, `testarm <N>`, `testdrop <pct>`, `data [from-index]`, `testdata`; exclusivity rejection that leaves the running cycle untouched (D11); `stop` routes to the runner first. Chunked CSV writer: max 256 B/tick gated by `Serial.availableForWrite()` (D8, NFR), and **suppress the 500 ms `printPosition()` line while a dump is in flight** (D14) or the CSV is unparsable. `data`/`testdata`/`pos` stay usable in Error, `home`/`home top`/`test` rejected (D15). New reply text as `Msg` rows only (D13) - **requires M2-T003 (`msg()`), #37 budget decision and #88 landed**
- [x] T013 [US3] [FR-007] `src/Diagnostics.{h,cpp}`: `kMaxChecks` 5 -> 7, `CHECK ENDSTOP1`/`CHECK ENDSTOP2` rows (HIGH at boot = WARN 0x0504 "pressed or broken wire - jog off the switch and re-run", never FAIL). **Also fix the latent cap in `src/WebUi.cpp` `statusJson()`**: `CheckStatus statuses[5]` + `i < 5` exclude checks 6+ from `overall` (plan §9) - size it from the check set, and bump `json.reserve()` with its stale "~740 B with 5 selftest checks" comment; verify `overall` on the bench with all 7 rows

## Verify / close

- [ ] T014 [US1] [US2] [US3] [TC-003] [TC-004] [SC-001] [SC-002] [SC-003] Bench matrix B1-B9 of plan §7 (blocked on #76 wiring + a test sample): over-travel bound documented and accepted, free-heap figure recorded, all four SC rows evidenced on the owning issues. Close milestone M3

## Dependencies

```
T001 T002 T003 T004 T005 T006 T007   (pure, independent of each other)
        |
        v
T008 (Endstops)  T009 (IMotion)  T010 (raw force)
        \            |            /
         v           v           v
              T011 TestRunner  (needs T001-T003 models)
                    |
                    v
              T012 Commands + CSV dump   (also: M2-T003 #33, #37, #88)
                    |
                    v
              T013 Diagnostics selftest rows
                    |
                    v
              T014 bench matrix (#76) -> close M3
```

Suggested order: T001 -> T002 -> T003 -> T004 -> T005 -> T006 -> T007 (all pure,
no hardware needed, unblocks nothing else in the repo), then T008-T013 while
#76 waits, then T014.
