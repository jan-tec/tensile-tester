# Feature Specification: M0 - Module Split

Status: Done (2026-08, #8-#19) | Rigor: Spec-Anchored | Created: 2026-08-20 | Depends on: -

**Goal:** Split `src/main.cpp` (1044 lines, monolith incl. inlined Web UI)
into single-responsibility modules following 2026 large-project best
practice. **Behavior-preserving**: same commands, same responses, same Web
API, same German UI (i18n is M2). Refactor, not rewrite.

## User Scenarios

### US1 (P1) - Single-responsibility modules

As a maintainer, I want each concern in its own module so I can change
commands or the Web UI without re-reading 1000 lines.

- Given the layout in plan.md section 2, When I add a command, Then only
  `Commands.*` (+ `FirmwareCore` if new parsing) changes.
- Given a Web UI tweak, When I edit it, Then only `WebUi.*` changes.

### US2 (P1) - Host-testable pure core

As a developer, I want all pure logic in `lib/FirmwareCore` so I can TDD
it without hardware.

- Given the input `rpm 5`, When `parseCommandValue` runs, Then it returns
  `5.0` on the host, no `Arduino.h`.

### US3 (P2) - Narrow hardware interfaces

As a test writer, I want app code to depend on `IMotion` / `ILoadCell`
boundaries so hardware adapters can be faked in tests (foundation for M1
diagnostics and future fake-hardware host tests).

## Requirements

### Functional

- **FR-001:** `main.cpp` becomes a composition root <= 150 lines (object
  wiring, `setup()`/`loop()` only).
- **FR-002:** Module split per plan.md section 2: `App`, `Motion`,
  `LoadCell`, `Settings`, `Commands`, `SerialCli`, `WebUi`.
- **FR-003:** Dependency direction: `src/` modules -> `lib/FirmwareCore`
  (pure C++: no `Arduino.h`, no `String`, no `PROGMEM`); no include cycles.
- **FR-004:** Behavior preserved: identical command set (`en off zero tare
  pos force stop rpm acc feed move turn jog cal_zero cal_apply`), identical
  response texts, identical Web endpoints (`/`, `/api/status`,
  `/api/command`), unchanged `INDEX_HTML`.
- **FR-005:** Stepper stays in a dedicated FreeRTOS task on core 0; all
  stepper access behind `portMUX`; no blocking in critical sections.
- **FR-006:** `INDEX_HTML` lives in `WebUi.cpp`, still `PROGMEM`.
- **FR-007:** `IMotion` (<= 8 methods) and `ILoadCell` (<= 5 methods)
  defined in `src/`; adapters implement them; `App` consumes via reference
  (constructor injection); no dynamic allocation.
- **FR-008:** Command parsing (`parseValueAfterCommand`) moves to
  `FirmwareCore` as a `char*`-based pure function; `Commands.cpp` keeps a
  thin `String` adapter.
- **FR-009:** `AGENTS.md` Architecture/Layout sections and `CHANGELOG.md`
  updated to the new structure.

### Constraints & TDD Targets

- **TC-001:** Every extraction is a REFACTOR step: `pio test -e native` +
  `pio run -e esp32dev` green before and after; target smoke spot-check per
  task (serial: `pos force feed move stop`; Web: `/api/status` + one
  command). AGENTS.md TDD rule, REFACTOR phase.
- **TC-002:** New pure logic (`parseCommandValue`) is RED-first:
  `test/test_command_parser.cpp` failing before implementation.
- **TC-003:** `FirmwareCore` coverage >= 90% (core/internal tier) - verified
  via the existing test suite (10 gear tests + new parser tests).

### Non-Functional

- No new PlatformIO libraries; RAM/flash delta < 2 KB (relocation, not
  duplication).
- No visible serial/Web behavior change at any intermediate step.

## Success Criteria

- **SC-001:** `main.cpp` <= 150 lines; no `src/` module > 350 lines.
- **SC-002:** Smoke checklist 100% identical before/after (full command set
  serial + Web UI browser pass).
- **SC-003:** All tasks merged with CI `test` + `build` green.

## Assumptions

- Arduino + PlatformIO retained (research decision, plan.md section 5, D5).
- One module per PR (Baby Steps, 1-3 files per step).

## Next Step

Execute `specs/M0-module-split/tasks.md` in dependency order, TDD rules per
AGENTS.md.
