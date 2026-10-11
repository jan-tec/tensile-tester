# Implementation Plan: M6 - Structured Serial API (JSON Lines)

Status: Draft | Spec: `spec.md` (FR-001..FR-006, TC-001..TC-005, SC-001..SC-003) |
Created: 2026-10-11 | Depends on: M2 (`msg()` locale tables), M3 (cycle/dump
infrastructure) | Unblocks: M5 (wizard prompts), M4 (`chold` responses)

## 1. Technical Context

- One command policy serves serial and web: `Commands::execute(String,
  String&) -> bool` (542 lines). Failure paths return `msg()` locale prose;
  no machine code exists for command failures today.
- Four serial emitters beyond command replies (all must be JSON or gone
  by SC-001):
  1. periodic status: `App::loop()` calls `commands_.printPosition()` every
     `kStatusIntervalMs` (multi-line prose; suppressed during dumps, D14)
  2. web-command echo: `WebUi::handleCommandApi()` prints `WebCommand` +
     command + response (3 prose lines per web command)
  3. `pos`: the policy itself prints the position block directly to Serial
  4. CSV dump chunks from `pollDump()` (M3 grammar, stays byte-exact)
- Web contract to preserve (FR-006): `/api/command` answers HTTP 200/400
  with `jsonMessage(ok, response)`; `/api/status` carries state + selftest.
- `ErrorCode.h` blocks 0x01xx..0x05xx are used; 0x06xx is free (append-only
  rule). `errorCodeName()` / `errorCodeHex()` are the stable grammar helpers.
- State snapshot inputs all exist: `motion_.positionMm()` (meaningful only
  when homed), `loadcell_.forceNewton()/isReady()/calibrationValid()`,
  `testRunner_.homed()`, `TestCycleModel::name(testRunner_.testState())`,
  `model.faultCode()`. `WebUi::statusJson()` already renders a similar
  object by hand (web-side; untouched this milestone).
- Bench fact (2026-10-11, #48 session): the periodic status line is
  2 lines of prose + a datum warning interleaved with command replies -
  exactly the scraping pain US1 removes.

## 2. Architecture

```
lib/FirmwareCore (pure C++, host-tested, no Arduino/String/PROGMEM)
  ErrorCode.h     + 0x06xx API block + names                    FR-004
  ApiResponse.h   StateSnapshot {posMm, forceN, calValid,
                  cycleState, faultCode, homed}
                  formatStateJson()        fixed order/decimals  FR-002
                  formatResponseLine()     {ok,data|error,state},
                                            ASCII escaping, <=256 B FR-001
                  formatDumpHeader/Footer() JSON framing         FR-005
  FirmwareCore.h  commandTable: FR-003 surface for TC-003 sweep

src (hardware + policy, target-tested)
  Commands        execute() gains ApiError& out-param (set at every
                  failure return, ~20 mechanical sites); pos data via
                  out-struct instead of Serial prints; printPosition()
                  replaced by snapshot building                 FR-001/002
  SerialCli       renders the envelope, single write + flush before
                  returning to the poll loop                    TC-005
  App             periodic status becomes one JSON event line
                  {"event":"status","state":{...}}              SC-001
  WebUi           web echo becomes {"event":"web_command",...};
                  /api/command + /api/status otherwise unchanged FR-006
  Text.h          +Msg rows (message text lives in error.message)
```

Flow (US1): host sends line -> `SerialCli` dispatches -> `execute()`
runs policy, sets `{bool ok, ApiError code, String text, PosData}` ->
`SerialCli` snapshots state once -> `formatResponseLine()` -> one write,
flush, next line. `data` (US3): envelope header line -> untouched CSV
chunks (existing `pollDump()` D8/D14 rules) -> footer line.

## 3. Key Decisions

| # | Decision | Rationale |
|---|----------|-----------|
| D1 | `execute()` keeps `(bool, String)` and gains an `ApiError&` out-param; serial uses the code, web ignores it | Web stays byte-identical (FR-006) without forking the policy into two paths; one site maps prose failures to codes |
| D2 | All Serial writes leave `Commands` (pos block, status line) and move to `SerialCli`/`App`; the policy computes, the surface renders | Layering; makes TC-003 (pure command-table sweep) possible at all |
| D3 | Async serial lines become JSON events with a top-level `"event"` key (`status`, `web_command`); BOOT/STATE/CHECK grammar untouched (spec A1) | SC-001 demands zero non-JSON lines; operators keep live feedback; parsers distinguish reply vs event by key |
| D4 | `data` framing: JSON header (count, format tag) -> byte-identical CSV body -> JSON footer (completion) | FR-005; TC-004 pins body byte-identity against the M3 formatter output |
| D5 | Phasing: M6 envelopes the CURRENT command surface; `chold*`/`calibrate`/`cal_next` (M4/M5) arrive envelope-native in their milestones | Resolves the spec's M4/M5/M6 dependency loop; nothing in the envelope is command-specific |
| D6 | `help` returns the command list as a JSON array in `data` - the documented >256 B exception (with the dump header/footer) | Spec FR-003/NFR |
| D7 | Hand-rolled `snprintf` emission, locale-neutral `%.3f`/`%.2f`, fixed key order pinned by exact-string host tests | Spec NFR (no JSON lib); host tests make the wire format a contract |
| D8 | New codes (0x06xx): UnknownCommand 0x0601, InvalidValue 0x0602, StateConflict 0x0603 (busy/error-state/dump exclusivity), NotHomed 0x0604, NotCalibrated 0x0605, EmptyCommand 0x0606 | Spec's initial set + empty-command (existing failure path with no other code); appended, never renumbered |

## 4. Task Sequence (issues cut from this table; RED-first where pure)

| Task | Content | Layer |
|------|---------|-------|
| M6-T001 | [RED] 0x06xx block: codes, `errorCodeName` rows, hex grammar | FirmwareCore |
| M6-T002 | [RED] `ApiResponse`: StateSnapshot + envelope formatter, escaping, 256 B bound, event lines (TC-001/002) | FirmwareCore |
| M6-T003 | [RED] dump header/footer formatters, body byte-identity (TC-004) | FirmwareCore |
| M6-T004 | [RED] FR-003 command-table sweep: every current command renders a parseable envelope incl. error paths (TC-003) | FirmwareCore |
| M6-T005 | `execute()` ApiError mapping at every failure return; `pos`/status restructure (D2); compile gate | src |
| M6-T006 | `SerialCli` envelope + flush; App status event; WebUi echo event (D3, TC-005) | src |
| M6-T007 | Web regression: `/api/command`, `/api/status` byte-identical (FR-006, bench) | bench |
| M6-T008 | Bench SC-001 pyserial sweep (json.loads every reply, zero stray lines) + SC-002/003; close #175 | bench |

## 5. Test Mapping

| TC | Test file (Unity, `pio test -e native`) |
|----|------------------------------------------|
| TC-001 | `test_api_response.cpp`: exact-string envelopes, escape cases, length bound |
| TC-002 | `test_api_response.cpp`: state combinations (unhomed, uncalibrated, fault, cycle) |
| TC-003 | `test_api_commands.cpp`: table sweep over the FR-003 surface |
| TC-004 | `test_api_dump.cpp`: framing around `formatCsvChunk` output, byte-identity |
| TC-005 | bench: flush-before-next verified by a rapid-fire pyserial script (M6-T008) |

## 6. Risks

- `execute()` signature change touches every command branch - mechanical
  but wide; mitigated by TDD layers landing first (T001-T004) and the
  compile gate.
- Prose-to-code mapping is lossy where one message served two causes
  (e.g. `CycleBusy` = active cycle vs dump in flight): StateConflict gets
  both initially; split codes only if a consumer needs it (append rule).
- ESP32 `snprintf("%.2f")` and host newlib agree on formatting; pinned by
  exact-string host tests + one bench spot check (T008) to catch drift.
- Serial output volume rises slightly (state object per reply); at 115200
  8N1 a <=256 B line is <=22 ms - no timing conflict with the 10 Hz loop.
