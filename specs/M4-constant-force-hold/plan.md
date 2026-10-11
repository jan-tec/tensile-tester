# M4 Plan - Constant-Force Hold (Creep-Style Test)

Status: Planned | Spec: `specs/M4-constant-force-hold/spec.md` (#174)
Depends on: M3 (homing, sampling, CSV dump, exclusivity), M6 (runner events).

## 1. Architecture (D1 - D7)

**D1 - One runner, two modes.** `chold` runs through the existing
`TestRunner` with a mode flag (`Tensile | Hold`), NOT a second runner.
Rationale: one actuation path (IMotion), one sample buffer, one CSV
dump, one exclusivity gate, one `stop` policy. The hold brain is a
pure `FirmwareCore::HoldController` (TC-001); TestRunner in Hold mode
only feeds it per-tick inputs and executes its outputs:

```text
tick: force N, position mm, endstops, limits, elapsed
        -> HoldController::update()
        <- {state, correctionMm (rate-limited), sample?, endReason?}
```

**D2 - HoldController is a pure FSM** (no Arduino, host-tested):
`Idle -> Homing -> Approach -> Holding -> Done | Limit | Lost | Aborted`.
`Error` is the device-level state (M1 model) - the controller reports
`fault` as an end reason, the DeviceModel owns the state.

**D3 - Control law (spec A4): deadband proportional with rate limit.**
Inside the tolerance band: no correction. Outside: correction per
10 Hz tick = `sign(error) * min(kAbs * |error|, kRateLimitMmPerTick)`,
outward on underforce, inward on overforce. `kRateLimitMmPerTick`
default 0.05 mm (= 30 mm/min machine cap, TC-003); `kAbs` default
0.002 mm/N/tick (tunable constant, pinned by test). The rate limit -
not the gain - dominates every large error, which is what makes the
100 ms HX711 lag stable.

**D4 - Approach (FR-002):** tension approaches upward at `testspeed`;
compression transits up by `choldclear` first (D26 during transit ->
start approach from there), then approaches downward. The approach
ends when |force| >= 50% |setpoint| (the same threshold doubles as
the FR-004 hold floor). Approach overshoot bound (TC-002): with
testspeed <= 30 mm/min, 2-sample latency (0.2 s) + loop decel via the
rate limit bound the first peak at |setpoint| + 10% - pinned on the
pure model with the worst-case constants.

**D5 - Lost detection (FR-004):** |force| < floor while the last
correction was outward, confirmed on 2 consecutive ticks (200 ms).
Single-tick dips inside the confirmation window do not abort.

**D6 - Buffer keep-last (FR-006, TC-004):** `SampleData` grows an
overwrite mode. In Hold mode a full buffer overwrites the oldest
sample and counts `overwritten`; M3 Tensile keeps its exact behavior
(stop-on-full, `truncated` flag). Progressive decimation: OUT (A3 -
bench-scale minutes at 10 Hz = 8192 s = 136 min covers the use case;
the overwrite count documents the loss).

**D7 - Config (FR-001):** `HoldConfig` fields ride the existing
`TestConfig` (single validation boundary `clampTestConfig`, NVS via
`Settings::saveTestConfig`): `holdTolerancePct` [1, 25] default 5,
`holdClearanceMm` [1, 100] default 20. Setpoint + duration stay
per-command (not persisted). `chold` setpoint magnitude clamps to
[1, 4000] N (floor above noise, ceiling below the 4903 N cell rating).

## 2. Surface changes (D8 - D10)

**D8 - Serial commands:** `chold <N> [minutes]` (0/omitted = until
`stop`), `choltol <pct>`, `choldclear <mm>` in `Commands::execute()`
with the M3 gates (exclusivity via `testRunner_.active()`, dump guard,
Error-state rejection). All replies through existing Msg rows + 4 new
rows (start, busy-alike reuse, hold-set, hold-clear-set) - locale
table EN/DE, ASCII in serial DE.

**D9 - M6 envelope:** new runner event
`{"event":"runner","data":{"kind":"hold_summary", ...}}` with
`endReason` (time|stop|limit|lost|fault), `heldS`, `meanN`, `maxDevN`,
`inBandPct`, `creepMm`, `overwritten` - `formatApiRunnerEvent`
extension, host-tested exact-string (M6 grammar style). `help` gains
the chold rows.

**D10 - Web (spec NFR: view-only):** `/api/status` `testState` shows
the hold states via the existing `TestCycleModel::name` contract
(extend: `Approach`, `Holding`); the run block carries the hold
summary facts after a run (view-only, no new controls - M7 Phase C
controls are a later, separate PR per #172).

## 3. Risks

| Risk | Mitigation |
|------|------------|
| Force sign inverted at the bench (A2) | cal_zero/cal_apply path re-verified in T008 before the first tension hold; compression SC-002 gated on A1 hardware |
| Loop oscillation on stiff samples | D3 rate limit + deadband; `choltol` tunable per sample; bench matrix in T008 sweeps tolerance |
| Overshoot bound depends on sample stiffness | TC-002 sweep proves the +10% bound for k <= 100 N/mm @ 50 N (k_bound = setpoint x 10% / 0.05 mm rate); stiffer fixtures documented as bench-tuning territory (threshold/choltol) |
| Chasing a lost sample to a limit | D5 Lost abort + both endstops remain hard bounds (Limit end reason) |
| Long holds overwrite data silently | `overwritten` count in summary + event (D6) |

## 4. Verification

Host: TC-001..TC-005 RED-first in `test/` (238 -> ~300 cases).
Bench: SC-001/SC-004 (T008, tension + CSV), SC-003 (T009, cut sample),
SC-002 (T009, compression - needs the operator's A1 arrangement).
Gates per AGENTS.md: native + esp32dev green before PR, CI before
merge, bench smoke for hardware behavior.
