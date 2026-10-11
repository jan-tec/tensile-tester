# Milestone Roadmap

Milestones for the tensile-tester firmware, Spec-Anchored (spec.md = WHAT,
plan.md = HOW, tasks.md = issue backlog). **One task = one issue = one PR.**

| MS | Title | Status | Depends on | Spec dir |
|----|-------|--------|------------|----------|
| **M0** | Module split (behavior-preserving refactor) | done | - | `specs/M0-module-split/` |
| **M1** | Startup selftest & health diagnostics | active | M0 | `specs/M1-startup-selftest/` |
| **M2** | i18n DE/EN, English primary | done | M1 | `specs/M2-i18n-de-en/` |
| **M3** | Automated tensile test cycle | active | M1, #71 | `specs/M3-tensile-test-cycle/` |
| **M4** | Constant-force hold | planned (plan/tasks ready 2026-10-11, T001-T009 = #205-#213) | M3 | `specs/M4-constant-force-hold/` |
| **M5** | Load cell calibration | planned | M3, M6 | `specs/M5-loadcell-calibration/` |
| **M6** | Serial JSON API | done (2026-10-11, #185-#194) | M2, M3 | `specs/M6-serial-json-api/` |
| **M7** | Web UI redesign | done (2026-10-10, #172/#173) | - | `specs/M7-web-ui-redesign/` |
| **M8** | LED status language (GPIO2) | active | M1, M3, #76, #128 | `specs/M8-led-status/` |

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

## Hardware status (2026-08-20, bench)

ESP32 connected via USB **only** - no GPIO wiring. Consequences:

- M1 is designed for missing peripherals: `NOT_CONNECTED` / `NOT_WIRED` are
  warnings, not failures (SC-001 of M1 verifies exactly this bench state).
- M1 power check needs a resistor divider (5 V rail -> GPIO34): hardware
  task M1-T008, blocked until built.
- M0 is pure refactor: fully implementable and verifiable with the ESP32 on
  USB (flash + serial + Web via USB/WLAN dongle).

## Method

- Spec skill: Spec-Anchored standard; FR/TC/SC per milestone.
- TDD skill: new pure logic RED-first in `lib/FirmwareCore` + `test/`
  (Unity host tests); refactors keep the suite green (REFACTOR phase).
- Research basis: 2026-08-20 deep research (citations in
  `specs/M0-module-split/plan.md` section 7).
