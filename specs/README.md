# Milestone Roadmap

Milestones for the tensile-tester firmware, Spec-Anchored (spec.md = WHAT,
plan.md = HOW, tasks.md = issue backlog). **One task = one issue = one PR.**

| MS | Title | Status | Depends on | Spec dir |
|----|-------|--------|------------|----------|
| **M0** | Module split (behavior-preserving refactor) | done | - | `specs/M0-module-split/` |
| **M1** | Startup selftest & health diagnostics | active (T008 divider hardware + T011 bench rows open) | M0 | `specs/M1-startup-selftest/` |
| **M2** | i18n DE/EN, English primary | done | M1 | `specs/M2-i18n-de-en/` |
| **M3** | Automated tensile test cycle | active (T014 bench matrix only) | M1, #71 | `specs/M3-tensile-test-cycle/` |
| **M4** | Constant-force hold | in progress (T001-T003, T005, T006 merged; T004, T007-T009 open) | M3 | `specs/M4-constant-force-hold/` |
| **M5** | Load cell calibration | partial (docs/calibration.md landed; guided wizard open) | M3, M6 | `specs/M5-loadcell-calibration/` |
| **M6** | Serial JSON API | done (2026-10-11, #185-#194) | M2, M3 | `specs/M6-serial-json-api/` |
| **M7** | Web UI redesign | done (2026-10-10, #172/#173) | - | `specs/M7-web-ui-redesign/` |
| **M8** | LED status language (GPIO2) | active (T004 operator passes only) | M1, M3, #76, #128 | `specs/M8-led-status/` |

## Issue tracking

GitHub Issues are **enabled** on this repo (2026-08-20, via owner-level API
`PATCH /repos/{owner}/{repo}` `has_issues=true`) and the backlog is synced
**1:1**:

| Milestone | GitHub issues |
|-----------|---------------|
| M0 | #8-#19 (T001-T012), #8 closed |
| M1 | #20-#30 (T001-T011) |
| M2 | #31-#38 (T001-T008) |
| M3 | #118-#157 (T003-T014 + NQP/RED stream) |
| M8 | #132-#136 (T001-T004) |
| M7 | #167-#172 (US2/US3 + spec + impl) |
| M4 | #205-#213 (T001-T009, milestone #11) |
| M6 | #177-#184 (T001-T008, all closed) |
| Quirks | #41-#43 (NQP stream, grows as implementation runs) |

**Numbering note (2026-10-11):** all numbers above predate the fork->canonical
migration and point at `satwareAG-ironMike/tensile-tester` (see the mapping
table in AGENTS.md "Issue/PR numbers"). Every issue that was open at the
migration was recreated verbatim in `jan-tec/tensile-tester` and the old one
closed with a pointer - the live backlog is jan-tec `#5`-`#16`. Open work:
`#5` M1-T008 divider, `#6` M1-T011 matrix, `#7` M0 cal_apply bench (quirk),
`#8` M5 weight wizard, `#9` web UI bench test, `#10` M8-T004 close,
`#11` M3-T014 matrix, `#12` M4 umbrella, `#13`-`#16` M4-T004/T007/T008/T009.
The `Quirks` milestone exists in jan-tec (open quirk issues: `#7`).

**NQP (No-Quirk Policy)**: every quirk found during implementation is pinned
by a test and filed as an issue in the `Quirks` milestone (label `quirk`);
each is fixed only by an explicit behavior-change PR. See AGENTS.md
"No-Quirk Policy (NQP)".

The `tasks.md` files remain the **canonical** backlog (dependency chains and
gates); GitHub issues are the tracking mirror (each body carries spec refs +
gate). Rule: when a `tasks.md` list changes, sync the issues in the same PR.

## Per-task gate (AGENTS.md policy)

Local `pio test -e native` + `pio run -e esp32dev` green **before opening a
PR**; CI `test` + `build` green **before merge**. Target smoke test where a
task touches hardware behavior.

## Hardware status (2026-10-11, bench)

Wired and bench-verified: motors + TMC2209 drivers (STEP/DIR/EN, RMT pulses),
HX711 load cell (10 Hz, calibrated 2026-10-11 with a 2465 g reference mass),
endstops GPIO 25/26 (staged homing reaches a `Travel -0.001 mm` datum,
B3/B5/B7 matrix rows evidenced), LED GPIO2 status language. Mainboard CAD:
`hardware/mainboard-atlas/` (KiCad 9; ENDSTOP1 = GPIO25 on J5).

Still open on the bench:

- 5 V rail divider -> GPIO34 is NOT built (M1-T008, jan-tec #5): POWER reads
  `NOT_WIRED` on a floating pad; two 10 k resistors are the remaining parts
  task.
- M4 bench evidence (SC-001..SC-004, jan-tec #15/#16) needs a surviving test
  sample; two samples broke below rating (grip stick-slip suspected).
- Calibration re-anchor with a second, heavier mass (jan-tec #7): the 2.465 kg
  point verifies the factor to ~0.3% but not linearity at the ~16 kg working
  load.

## Method

- Spec skill: Spec-Anchored standard; FR/TC/SC per milestone.
- TDD skill: new pure logic RED-first in `lib/FirmwareCore` + `test/`
  (Unity host tests); refactors keep the suite green (REFACTOR phase).
- Research basis: 2026-08-20 deep research (citations in
  `specs/M0-module-split/plan.md` section 7).
