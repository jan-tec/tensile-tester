# Feature Specification: M2 - i18n DE/EN, English Primary

Status: Done (2026-08, #31-#38) | Rigor: Spec-Anchored | Created: 2026-08-20 | Depends on: M1

**Goal:** Bilingual (DE/EN) user interface. The device **starts in
English**; the operator can switch to German (persisted). In this milestone
the **project primary language becomes English**: code, Web UI, serial CLI,
and project docs (AGENTS.md, README, CHANGELOG) are English-first. Boot logs
(FR-007 of M1) are already English and stay untouched.

## User Scenarios

### US1 (P1) - English first boot

As an English-speaking operator, I want the Web UI and serial CLI to be in
English after a fresh boot.

- Given a factory-fresh device (no locale in NVS), When it boots, Then all
  UI text (Web + serial) is English.
- Given locale `en` stored, When it boots, Then it boots in English.

### US2 (P1) - German switch

As a German-speaking operator, I can switch the interface to German and it
remains German after reboot.

- Given locale `en`, When I run `lang de` (serial) or pick German (Web UI),
  Then UI text switches to German and the choice survives reboot (NVS).

### US3 (P2) - Single text source

As a maintainer, I want every user-facing string routed through one text
module (message IDs), so a locale is a table, not a code change.

- Given any user-facing message, When I grep for German literals in
  `src/`, Then I find none (logs excepted - they are English ops text).

## Requirements

### Functional

- **FR-001:** `src/Text.{h,cpp}`: `enum class Msg` (all user-facing
  messages: command echoes, errors incl. M1 codes, Web UI labels/status
  strings) + `PROGMEM` string tables `EN` (primary) and `DE` (secondary);
  `msg(Msg) -> const char*` resolves the active locale.
- **FR-002:** Locale model: `en` | `de`, default `en` (NVS key `locale`,
  namespace `testmachine`); stored via `Settings`.
- **FR-003:** Switch paths: serial command `lang <en|de>`; Web UI selector
  (persisted via `/api/command`); both take effect immediately.
- **FR-004:** English-first conversion (behavior-preserving): all current
  German UI/CLI strings become the **EN** table content (translated to
  English), DE table carries the existing German texts. Web UI: static
  labels rendered from locale (JS fetches `?locale=` or includes the table
  in `/api/status`; decision in plan.md D3).
- **FR-005:** Web UI serves `lang="en"` / `lang="de"` + UTF-8 (meta charset
  already present); DE strings use native umlauts in the Web UI.
- **FR-006:** Serial output DE locale uses ASCII transcription (ae/oe/ue/ss)
  to keep terminal output robust; EN is plain ASCII.
- **FR-007:** M1 error presentation: `ErrorCode` -> `Msg` mapping so
  localized messages resolve through `Text`; log lines unchanged (English).
- **FR-008:** Project language switch: `AGENTS.md`, `README.md`,
  `CHANGELOG.md` converted to English primary (existing German entries
  translated, history preserved via git); the AGENTS.md convention
  "German text is ASCII-transcribed" is replaced by "UI text is
  locale-table driven; serial DE uses ASCII transcription".

### Constraints & TDD Targets

- **TC-001:** `Text` resolution logic (locale select, fallback to EN for
  missing DE entries, default `en` on missing NVS key) host-tested:
  extract the resolution as a pure `FirmwareCore::Locale`
  (`resolve(Msg, Locale) -> table index`) - RED-first,
  `test/test_locale.cpp`.
- **TC-002:** Table completeness check: a host test asserts every `Msg`
  has a non-empty EN **and** DE entry (catches half-translated tables).
- **TC-003:** Target matrix: fresh boot (no locale) -> EN; `lang de` -> DE
  + survives reboot; Web selector -> DE + survives; error path shows
  localized message (e.g. M1 `POWER_LOW`).

### Non-Functional

- Tables `PROGMEM`; RAM delta < 1 KB; no new libraries.
- String table size budget: EN + DE combined < 4 KB flash (measured in
  build output, reported in the milestone-close PR).

## Success Criteria

- **SC-001:** Fresh device boots with fully English Web UI + serial CLI.
- **SC-002:** `lang de` / Web selector switches both surfaces; persisted
  across reboot (TC-003).
- **SC-003:** Zero user-facing German literals in `src/` outside
  `Text.{h,cpp}` (grep-verified; logs excepted).

## Assumptions

- EN is the language of record for docs/commits from this milestone on.
- M1 is merged first (error `Msg` mapping builds on `ErrorCode`).
- No third locales now; the table design admits them later (add a column).

## Next Step

Execute `specs/M2-i18n-de-en/tasks.md` in dependency order (after M1 is
merged).
