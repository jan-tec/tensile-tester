# Implementation Plan: M1 - Startup Selftest & Health Diagnostics

## 1. Technical Context

Builds on M0 modules: `App`, `Motion`, `LoadCell`, `Settings`, `Commands`,
`WebUi`, `FirmwareCore`. Pins: DIR 19, STEP 18, EN 5, HX711 DOUT 32 / SCK
33. **Free ADC pins:** 34, 35, 39 (input-only). ESP32 reset-reason and
free-heap APIs available in the Arduino core.

## 2. Architecture

```
main.cpp (composition root)
  -> App
       -> Diagnostics (new, src/)
            -> DeviceModel (lib/FirmwareCore, pure state machine)
            -> HealthEval  (lib/FirmwareCore, pure threshold/hysteresis/variance)
            -> adapters: LoadCell (HX711 wait_ready + samples), PowerSense (ADC GPIO34), Motion (EN toggle), Settings (NVS open)
```

Boot flow: `setup()` -> `Diagnostics::runBootSelfTest()` (blocking, <= 5 s,
prints `CHECK` lines) -> `DeviceModel` event `SELFTEST_DONE` ->
`Idle` (start status loop, Web UI) or `Error` (driver off, report only).
Runtime: `App::loop()` ticks `Diagnostics::tickRuntime()` at 1 Hz
(power check with hysteresis) and feeds `POWER_LOW` events into the model.

## 3. Key Decisions

| # | Decision | Rationale |
|---|----------|-----------|
| D1 | State machine in `FirmwareCore` (pure), adapters in `src/` | M0 dependency rule; 100% host-testable (TC-001) |
| D2 | Machine codes are a stable `enum class ErrorCode : uint16_t` + string names; human messages are presentation (M2) | Codes survive localization; grep-able in logs |
| D3 | `NOT_CONNECTED`/`NOT_WIRED` are WARN/info, not FAIL | Current bench has no wiring; absence must not block motion (SC-001, SC-003) |
| D4 | Power sense on GPIO34 via 10 k/10 k divider, not `VSYS` | VSYS reflects 3.3 V domain; the 5 V rail under load is what matters |
| D5 | Motor check = EN toggle + `NO_FEEDBACK` report | No sense wiring exists; claiming OK would be a false positive |
| D6 | Boot log grammar fixed in FR-007 and English-only | Logs are ops text, not user-facing; M2 does not touch logs |
| D7 | HX711 "connected" = `wait_ready` succeeds within 1 s (plausibility + variance as WARN diagnostics) | `wait_ready` timeout is the robust absent/connected discriminator |

## 4. Check Table (expected on current bench, SC-001)

| id | Check | Expected (no wiring) | Status |
|----|-------|----------------------|--------|
| BOARD | reset reason, free heap > 30 KB, flash 4 MB | values logged | OK |
| NVS | `Preferences` open, calibration config sane | opens (first boot: NOT_FOUND -> defaults) | OK |
| POWER | GPIO34 divider | not wired | NOT_WIRED (info) |
| HX711 | `wait_ready` 1000 ms + 500 ms variance window | load cell wired, no load change | WARN 0x301 NO_VARIATION; NOT_CONNECTED only if the load cell is absent |
| MOTOR | EN toggle | toggles | OK (NO_FEEDBACK note) |
| overall | - | warnings only | **Idle** |

Observed on the current bench 2026-08-21 (no wiring): matches the table
above, `Idle` at t=1.040 s. Full capture + SC-001 correction note:
`docs/hardware-power-check.md` §TC-003(a).

## 5. TDD Mapping

| Task | Phase | RED artifact |
|------|-------|--------------|
| T001 | New logic | `test/test_device_model.cpp` (all transitions + fault paths) |
| T002 | New logic | `test/test_health_eval.cpp` (thresholds, hysteresis, variance) |
| T003-T011 | REFACTOR/integration | existing suite green + target matrix (TC-003) |

Coverage: TC-001/TC-002 give `FirmwareCore` the bulk of new logic;
adapter code (ADC reads, HX711 calls) stays thin and target-verified.

## 6. Risks

| Risk | Mitigation |
|------|-----------|
| HX711 `wait_ready` timing on cold start | 1 s budget inside the 5 s selftest budget; measured on target |
| ADC noise on unwired pin (floating GPIO34) | `NOT_WIRED` detection = out-of-range/monotonic reading; pin config `INPUT_PULLDOWN`; document in D4 |
| `Error` state wedges the machine | Reset/reboot recovery is explicit (FR-009); driver default-off on boot |
| Power check false FAIL from divider tolerance | 10 % resistors + thresholds 4.50/4.75 V give margin; hysteresis + 5-sample debounce |

## 7. Research Citations (2026-08-20, Perplexity, high context)

| Topic | Source |
|-------|--------|
| State machine patterns for embedded devices | blog.mbedded.ninja: design patterns |
| ESP32 reset reasons | docs.espressif.com: ESP-IDF system reboot reasons |
| ADC1 pin constraints (34/35/39 input-only) | docs.espressif.com: ESP32 ADC |
| NVS open/failure semantics | docs.espressif.com: Preferences / NVS |
| Watchdog/brownout context for boot diagnostics | docs.espressif.com: monitoring & debugging |
