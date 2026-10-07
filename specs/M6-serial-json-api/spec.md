# Feature Specification: M6 - Structured Serial API (JSON Lines)

Status: Draft | Rigor: Spec-Anchored | Created: 2026-10-07 | Depends on: M2
(English message strings), M3, M4 (command surface), M5 (calibration
commands)

**Goal:** Every serial command responds with exactly one machine-parseable
JSON line ({ok, data|error, state}), so a host computer can drive the full
machine - movement, test cycles, calibration, readout - without scraping
human-readable text. USB serial only; the WiFi web surface is unchanged.
CSV data dumps stay CSV.

## User Scenarios

### US1 (P1) - Scripted movement control

As a host computer, I command motion and get a structured ack I can parse.

- **Given** an idle machine, **When** I send `move 5` over serial, Then I
  receive one JSON line with `ok:true`, a data payload (accepted move
  parameters), and a state object (position, force, cycle state, fault
  code) - no human text lines around it.
- **Given** the same command with an invalid value, **When** parsed, Then
  the reply is `ok:false` with a stable error code and message; the
  machine state is unchanged.
- **Given** a rejected command (motion in Error state, cycle exclusivity),
  **When** parsed, Then the rejection reason is in the error payload - the
  host never has to guess from prose.

### US2 (P1) - Full command set speaks JSON

As a host computer, the entire command surface behaves uniformly.

- **Given** any command in the set (readout, motion, calibration, test and
  hold cycles, data dumps), **When** sent over serial, Then the response
  follows the same envelope rules - no command-specific text formats.
- **Given** an unknown command, **When** sent, Then the reply is a
  structured unknown-command error including the accepted command list.

### US3 (P2) - Bulk data stays CSV

As a host computer, I still pull measurement series as CSV.

- **Given** samples in memory, **When** I run `data`, Then the dump remains
  raw CSV lines with an end marker (M3 contract), bracketed by JSON lines
  (header: count + format; footer: completion) so a parser can frame it.

## Requirements

### Functional

- **FR-001 (Response envelope):** every serial command produces exactly
  one newline-terminated JSON line:
  `{ok: true, data: {...}, state: {...}}` or
  `{ok: false, error: {code: <int>, message: <string>}, state: {...}}`.
  Pure ASCII, no pretty-printing, key order fixed as documented. The shape
  mirrors the satware harness command-output contract ({ok, data, error}).
- **FR-002 (State object):** `state` carries at minimum: position mm,
  force N, calibration-valid flag, cycle state name, fault code, homed
  flag. Present in every response (single source: one snapshot per
  command).
- **FR-003 (Full command set):** all existing and planned commands speak
  the envelope: `en` `off` `zero` `tare` `pos` `force` `stop` `rpm` `acc`
  `feed` `move` `turn` `jog` `help` `cal_zero` `cal_apply` `calibrate`
  `cal_next` `home` `home top` `test` `testspeed` `testarm` `testdrop`
  `data` `testdata` `chold` `choltol` `choldclear`. `help` returns the
  command list as a JSON array in `data`.
- **FR-004 (Stable error codes):** API error codes live in the stable
  vocabulary (`ErrorCode.h` rule: append, never renumber); message strings
  are English (M2), error codes are the machine contract. Machines parse
  `error.code` only - `message` is display text and may localize with the
  active locale.
- **FR-005 (Bulk data exception):** `data` keeps the M3 CSV contract:
  one JSON header line (sample count, format tag), raw CSV lines, end
  marker, one JSON footer line (completion status). No other command
  emits non-JSON lines.
- **FR-006 (Web unchanged):** `/api/command` and `/api/status` keep their
  current behavior (human/legacy text). The web UI is not migrated this
  milestone.

### Constraints & TDD Targets

- **TC-001:** envelope formatting and parsing is a pure `FirmwareCore`
  module (command result in -> JSON line out; fixed key order, ASCII,
  escaping rules), host-tested RED-first including error cases.
- **TC-002:** state snapshot serialization is pure and host-tested
  (position/force/cycle/fault combinations).
- **TC-003:** every command in FR-003 is host-tested through the pure
  command policy + formatter: response parses as JSON and carries the
  documented envelope (exhaustive coverage of the command table).
- **TC-004:** CSV framing (header/body/footer) host-tested on the pure
  formatter; body bytes untouched.
- **TC-005:** serial timing: a response line must flush before the next
  command is processed (line-oriented request/response; no interleaving).
  Target-tested at the bench.

### Non-Functional

- Response lines <= 256 bytes (state object is fixed-width-ish); long
  payloads (CSV body, help list) are the documented exceptions.
- Hand-rolled JSON emission (snprintf) preferred over a JSON library - the
  structures are flat and the envelope is fixed; no new libraries unless
  the plan proves escaping is error-prone.
- Serial remains 115200 8N1.

## Key Entities & Data Model

- **ApiResponse:** ok (bool), data (object, on success), error (code int +
  message string, on failure), state (object, always).
- **MachineState:** pos mm, force N, calValid, cycleState, faultCode, homed.
- **Error codes:** appended to the stable vocabulary; unknown-command,
  invalid-value, state-conflict (cycle exclusivity), not-homed,
  not-calibrated are the initial entries.

## Assumptions & Handoffs

- **A1:** serial output becomes JSON-only for command responses (the
  English message strings live on inside `error.message` and log lines -
  the M1 BOOT/STATE/CHECK log grammar is untouched). Operators reading the
  raw terminal see JSON; the human-friendly surface stays the web UI.
  Ordering: M6 depends on M2-T001 + T002 only (Locale + Text skeleton, so
  messages resolve through `msg(Msg)`); M2-T003's echo-conversion half is
  superseded by this milestone - its `lang` command + NVS locale halves
  stay.
- **A2:** the web `/api/command` keeps its legacy format this milestone; a
  later milestone may unify web + serial on the same envelope (out of
  scope).
- **A3:** JSON-over-serial MCU protocols are established prior art
  (Klipper's host-MCU messaging); this spec's envelope is deliberately
  simpler (one line per command, no compression, no dictionary phase).
- **Next Step:** `/spec.plan` together with M5.

## Success Criteria

- **SC-001:** a pyserial host script sends every command in FR-003 and
  successfully `json.loads` each response; zero non-JSON lines outside the
  documented CSV dump framing.
- **SC-002:** `move 5` / `home` / `chold 50 1` driven remotely show the
  state object advancing (position, cycle state) across responses.
- **SC-003:** an unknown command and an Error-state motion attempt both
  return stable, documented error codes without disturbing machine state.
