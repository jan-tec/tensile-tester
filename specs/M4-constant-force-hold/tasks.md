# M4 Tasks - Constant-Force Hold

Backlog (canonical). 1 task = 1 issue = 1 PR. Sync rule: when this
list changes, sync the GitHub issues in the same PR (AGENTS.md).

## Chain

- [x] **T001** HoldConfig in TestConfig (D7): `holdTolerancePct`
      [1,25] d5, `holdClearanceMm` [1,100] d20 through
      `clampTestConfig` + `defaultTestConfig`; NVS round-trip via
      Settings; parser entries `chold/choltol/choldclear` (value
      grammar, signed setpoint). Host: clamp + parser tests RED-first.
      Issue: M4-T001
- [x] **T002** HoldController pure FSM (D2, TC-001): states, approach
      threshold = 50% |setpoint|, Lost with 2-tick confirm (D5), end
      reasons time/stop/limit/lost/fault, duration accounting.
      Host: transition-table tests incl. collapse + limit.
      Issue: M4-T002
- [x] **T003** Control law + accounting (D3, TC-003/TC-002/TC-005):
      deadband + rate-limited correction (0.05 mm/tick cap), overshoot
      bound proof on the pure model, mean/maxDev/inBand/creep pure
      functions. Host: synthetic-series tests.
      Issue: M4-T003
- [ ] **T004** Buffer keep-last (D6, TC-004): SampleData overwrite
      mode + `overwritten` counter; M3 behavior unchanged.
      Host: 8192-cap ring test.
      Issue: M4-T004
- [x] **T005** TestRunner Hold mode (D1): mode flag, Homing reuse,
      approach actuation (D4 incl. compression transit + D26 bound),
      per-tick controller drive, endstop Limit, power fault -> fault
      end reason, stop. Target smoke: first bench hold.
      Issue: M4-T005
- [ ] **T006** Command surface + envelope (D8/D9): execute() gates,
      Msg rows (EN/DE ASCII-safe), help rows, `hold_summary` runner
      event in ApiResponse + state snapshot fields. Host: formatter
      exact-string tests + locale coverage gate.
      Issue: M4-T006
- [ ] **T007** Web view-only (D10): cycleState names Approach/Holding,
      hold facts in the /api/status run block. Host: UiHtml/statusJson
      pins. Bench: view-only during a hold.
      Issue: M4-T007
- [ ] **T008** Bench SC-001 + SC-004: `chold 50 5` tension on a sample,
      summary within tolerance, CSV plot shows approach/settle/hold +
      creep. Includes the A2 force-sign verification first.
      Issue: M4-T008
- [ ] **T009** Bench SC-003 + SC-002: cut-the-sample -> `lost` (motor
      stopped, no limit contact); compression cycle on the operator's
      A1 arrangement. Close #174 + M4 milestone.
      Issue: M4-T009

## Order

T001 -> T002 -> T003 -> T005; T004 parallel to T002/T003; T006 after
T005; T007 after T006; T008/T009 bench (operator presence).

## Scope guards

- No new libraries, no new hardware (spec NFR).
- M3 Tensile behavior unchanged (T004 keeps stop-on-full + truncated).
- Web stays view-only (M7 Phase C controls = separate PR, #172).
