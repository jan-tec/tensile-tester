# Tasks: M2 - i18n DE/EN, English Primary

Gate per AGENTS.md: local `pio test -e native` + `pio run -e esp32dev`
green before PR; CI green before merge; target matrix TC-003 where a task
touches user-facing text. **1 task = 1 PR.**

- [ ] T001 [RED] `FirmwareCore` `Locale`: `test/test_locale.cpp` failing first (resolve(Msg, Locale) -> table index, default `en` on missing key, fallback EN for missing DE entry) -> pure implementation. Gate: `pio test -e native`
- [ ] T002 [RED] `src/Text.{h,cpp}` skeleton: `enum Msg` (enumerate ALL current user-facing strings: command echoes, M1 error messages, Web labels/status) + EN/DE `PROGMEM` tables + completeness test `test/test_text_tables.cpp` (TC-002). EN = translated English of current German texts; DE = current German texts. Gate: `pio test -e native` + `pio run`
- [ ] T003 [REFACTOR] Message strings to English + locale switch: `lang <en|de>` command; `Settings` NVS `locale` key (default `en`); `Commands.cpp` message strings route through `msg(Msg)` (English default). NOTE: the serial text-echo layer is superseded by M6 (`specs/M6-serial-json-api/spec.md` A1) - T003 ships the strings + locale switch only; the echo-channel conversion is dropped. Target: message strings resolvable EN/DE; locale persisted
- [ ] T004 [REFACTOR] M1 error presentation: `ErrorCode -> Msg` mapping; localized messages on serial + `/api/status` (logs unchanged, D6). Target: `POWER_LOW`-style message localized in EN and DE
- [ ] T005 [REFACTOR] Web UI to English: `INDEX_HTML` static labels -> English + embedded JS i18n dict (EN + DE, D3); `locale` field in `/api/status`; language selector (EN/DE) in the UI; `lang="en|de"` + UTF-8 (FR-005). Target: fresh boot -> English UI (SC-001, part 2)
- [ ] T006 [REFACTOR] Locale switching live: Web selector + `lang` command take effect immediately on both surfaces; persistence across reboot (TC-003: en->de->reboot->de; de->en->reboot->en)
- [ ] T007 [VERIFY] Full target matrix TC-003 + flash budget report (EN+DE < 4 KB) + SC-001/SC-002
- [ ] T008 [DOC] Project language switch (FR-008): `AGENTS.md` (replace ASCII-transcription convention with locale-table rule; convert to English), `README.md`, `CHANGELOG.md` (translate, history stays in git); SC-003 grep gate (zero user-facing German literals outside `Text.*`); close milestone

Dependencies: T001 -> T002 -> T003 -> T004 -> T005 -> T006 -> T007 -> T008.
