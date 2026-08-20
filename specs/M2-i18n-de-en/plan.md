# Implementation Plan: M2 - i18n DE/EN, English Primary

## 1. Technical Context

Builds on M0 modules + M1 `ErrorCode`. Web UI is the `INDEX_HTML` `PROGMEM`
string in `WebUi.cpp` (~480 lines of HTML/JS). Serial CLI responses are
`String` literals in `Commands.cpp`. German is currently ASCII-transcribed
(`Waegezelle`, `gueltige`) per the old AGENTS.md convention.

## 2. Architecture

```
src/Text.h/.cpp            # enum Msg + PROGMEM tables (EN primary, DE)
lib/FirmwareCore/Locale    # pure resolution: resolve(Msg, Locale) -> index, completeness
src/Settings               # NVS key "locale" (en|de), default en
src/Commands               # executeCommand: all echoes via msg(Msg); new "lang" command
src/WebUi                  # INDEX_HTML: static labels via JS i18n dict; lang selector
src/Diagnostics (M1)       # ErrorCode -> Msg mapping (localized presentation only)
```

Flow: operator sets locale (serial `lang de` or Web selector) ->
`Settings` persists -> `Text::setLocale()` -> all subsequent `msg()` calls
and the Web UI (next poll/reload) render in the new locale.

## 3. Key Decisions

| # | Decision | Rationale |
|---|----------|-----------|
| D1 | EN primary, DE secondary (not DE-primary + EN added) | User requirement: start in EN; project primary language becomes English |
| D2 | `enum Msg` + two `PROGMEM` tables, not `sprintf`-formatted C strings | Embedded i18n standard (Flash-only, RAM-safe); avoids format-string locale bugs |
| D3 | Web UI i18n: embed a small JS dict (EN + DE) in `INDEX_HTML` + `locale` field in `/api/status`; JS swaps labels client-side, no page reload | Web UI is one PROGMEM blob; a full server-rendered re-template doubles flash for a 12-label UI. Dict swap is < 1 KB. Re-verify in T006 |
| D4 | Serial DE = ASCII transcription (ae/oe/ue/ss) | 115200 terminal robustness; keeps existing convention for serial only |
| D5 | Web DE = native umlauts (UTF-8) | Browser renders UTF-8 perfectly; meta charset already set |
| D6 | Logs stay English-only (M1 FR-007), not localized | Logs are ops text; localizing them would pollute the grep-able grammar |
| D7 | Table-completeness host test (TC-002) | Catches half-translated tables at build time, not on the bench |
| D8 | `Msg` enum is the single source: no other user-facing literal allowed (SC-003 grep gate) | Prevents i18n rot |

## 4. TDD Mapping

| Task | Phase | RED artifact |
|------|-------|--------------|
| T001 | New logic | `test/test_locale.cpp` (resolution, default-en, fallback) |
| T002 | New logic | `test/test_text_tables.cpp` (completeness: every Msg non-empty EN+DE) |
| T003-T007 | REFACTOR | existing suite green + target matrix TC-003 |
| T008 | DOC/conversion | grep gate SC-003 + docs review |

## 5. Risks

| Risk | Mitigation |
|------|-----------|
| `INDEX_HTML` JS dict drift from server locale | `/api/status` carries `locale`; JS falls back to EN dict on unknown value; TC-003 covers the round trip |
| Table bloat (flash) | Budget < 4 KB (NF item); measured in build output at T007 |
| Missed literal during EN conversion | SC-003 grep gate (`rg '"[A-Za-z].*(?i)(ue|ae|oe)\b|[Ww]aegezelle' src/`) at T008 |
| `PROGMEM` + `String` mixing in `msg()` | `msg()` returns `const char*`; callers wrap in `String` as today (no API change) |

## 6. Research Citations (2026-08-20, Perplexity, high context)

| Topic | Source |
|-------|--------|
| PROGMEM string tables for embedded i18n | AVR/ESP32 `F()` / `PROGMEM` patterns (docs.espressif.com, Arduino references) |
| Locale persistence via NVS | docs.espressif.com: Preferences |
| Client-side dictionary swap for single-page Web UIs | standard SPA i18n pattern (e.g. i18next dict approach) |
