# Tasks: M1 - Startup Selftest & Health Diagnostics

Gate per AGENTS.md: local `pio test -e native` + `pio run -e esp32dev`
green before PR; CI green before merge; target verification per TC-003
where a task touches hardware. **1 task = 1 PR.**

- [x] T001 [RED] `FirmwareCore` `DeviceModel`: `test/test_device_model.cpp` failing first (states `Booting/SelfTest/Idle/Error`, events `SELFTEST_START/CHECK_RESULT/SELFTEST_DONE/POWER_LOW/RESET`, fault paths disable motor) -> pure implementation. Gate: `pio test -e native`
- [x] T002 [RED] `FirmwareCore` `HealthEval`: `test/test_health_eval.cpp` failing first (threshold eval 4.50/4.75 V, hysteresis, >= 5-sample debounce, variance window) -> pure implementation. Gate: `pio test -e native`
- [x] T003 `src/ErrorCode.h`: `enum class ErrorCode : uint16_t` (0x01xx NVS, 0x02xx power, 0x03xx HX711, 0x04xx motor) + `status` enum + `CheckResult` struct + stable string names
- [x] T004 `src/Diagnostics.{h,cpp}`: boot log grammar (`BOOT`/`CHECK`/`STATE` per FR-007) + `BOARD` check (reset reason, free heap, flash) + `NVS` check
- [x] T005 `Diagnostics`: `HX711` check (FR-006: `wait_ready` 1000 ms -> `NOT_CONNECTED`; plausibility + 500 ms variance) + force `n/a` handling in `LoadCell`/`App`/`statusJson`
- [x] T006 `Diagnostics`: `MOTOR` check (EN toggle, `NO_FEEDBACK` note per plan D5)
- [x] T007 `Diagnostics`: `POWER` check - `PowerSense` ADC on GPIO34 (`INPUT_PULLDOWN`), `NOT_WIRED` detection, runtime 1 Hz tick with hysteresis (FR-005); target: `NOT_WIRED` on bench
- [ ] T008 [HARDWARE, user] Build + wire 10 k/10 k divider 5 V rail -> GPIO34; verify with multimeter (expect ~2.5 V). Blocked until built; code path until then is `NOT_WIRED`
- [x] T009 `App` integration: `setup()` runs `runBootSelfTest()` before idle; `DeviceModel` drives `Idle`/`Error`; `Error` behavior per FR-009 (driver off, motion commands rejected with code + message, reboot recovery)
- [x] T010 Presentation: `selftest` object in `/api/status` (FR-008), serial boot report, Web UI selftest panel (German ASCII labels)
- [ ] T011 [VERIFY] Target matrix TC-003: (a) bench no-wiring -> plan.md check table (SC-001); (b) HX711 wired -> `OK`; (c) after T008 -> `POWER OK`. Docs: `docs/hardware-power-check.md`, AGENTS.md, CHANGELOG; close milestone

Dependencies: T001 -> T002 -> T003 -> T004 -> T005 -> T006 -> T007 ->
T009 -> T010 -> T011. T008 independent (hardware, parallel).
