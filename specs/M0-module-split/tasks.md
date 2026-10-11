# Tasks: M0 - Module Split

Gate for every task (AGENTS.md): local `pio test -e native` +
`pio run -e esp32dev` green before PR; CI `test` + `build` green before
merge. **1 task = 1 PR.** Strict chain (same-file refactor sequence).

- [x] T001 Research + spec freeze (this spec) - done 2026-08-20
- [ ] T002 [RED] `FirmwareCore` command parser: `test/test_command_parser.cpp` failing first, then pure `char*` implementation (`parseCommandValue(const char* input, const char* prefix, float* out)`, command-name extraction) in `lib/FirmwareCore`. Gate: `pio test -e native`
- [ ] T003 [REFACTOR] Extract `src/Motion.{h,cpp}`: AccelStepper, motor task (core 0), `portMUX`, enable/disable, `startMoveTo`, position/running, `applyMotionSettings`. Gate: + target smoke `en/off/rpm/feed/move/stop`
- [ ] T004 [REFACTOR] Extract `src/LoadCell.{h,cpp}`: HX711 begin, 10 Hz sampling, low-pass, `readAverageRawValue`, `updateForceMeasurement`, `tareLoadCell`, calibration compute. Gate: + target smoke `force/tare/zero`
- [ ] T005 [REFACTOR] Extract `src/Settings.{h,cpp}`: NVS load/save of calibration factor + valid flag (namespace `testmachine`). Gate: boot log unchanged
- [ ] T006 [REFACTOR] Extract `src/Commands.{h,cpp}`: `executeCommand` policy + command list; use T002 parser via `String` adapter. Gate: + target smoke full command set
- [ ] T007 [REFACTOR] Extract `src/SerialCli.{h,cpp}`: serial input loop. Gate: + target smoke serial
- [ ] T008 [REFACTOR] Extract `src/WebUi.{h,cpp}`: WebServer, `INDEX_HTML` (`PROGMEM`), `/`, `/api/status`, `/api/command`, `statusJson`. Gate: + target smoke Web (curl + browser)
- [ ] T009 [REFACTOR] Extract `src/App.{h,cpp}` (orchestration + loop); slim `main.cpp` to composition root (<= 150 lines, SC-001). Gate: full smoke
- [ ] T010 [REFACTOR] `IMotion`/`ILoadCell` interfaces (plan.md section 3) + reference injection into `App`; verify zero dynamic allocation. Gate: full smoke
- [ ] T011 [DOC] Update `AGENTS.md` (Layout + Architecture) and `CHANGELOG.md` (M0 entry)
- [ ] T012 [VERIFY] Full target smoke: complete command set over serial + Web UI browser pass (playwright-cli headed) + close milestone

Dependencies: T002 -> T003 -> T004 -> T005 -> T006 -> T007 -> T008 -> T009
-> T010 -> T011 -> T012.
