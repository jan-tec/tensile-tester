# Milestone Roadmap

Milestones for the tensile-tester firmware, Spec-Anchored (spec.md = WHAT,
plan.md = HOW, tasks.md = issue backlog). **One task = one issue = one PR.**

| MS | Title | Status | Depends on | Spec dir |
|----|-------|--------|------------|----------|
| **M0** | Module split (behavior-preserving refactor) | planned | - | `specs/M0-module-split/` |
| **M1** | Startup selftest & health diagnostics | planned | M0 | `specs/M1-startup-selftest/` |
| **M2** | i18n DE/EN, English primary | planned | M1 | `specs/M2-i18n-de-en/` |

## Issue tracking

GitHub **Issues are disabled** on this repo (`has_issues: false`, verified
2026-08-20; Projects enabled). The `tasks.md` files are therefore the
canonical issue backlog. If Issues get enabled in the repo settings, sync
`T###` tasks 1:1 to issues (title = task line, body = spec refs), one issue
per milestone group.

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
