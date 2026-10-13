# Implementation Plan: H1 - ATLAS Annotation Debt Fix

## 1. Technical Context

`hardware/mainboard-atlas/{Mainboard-ATLAS.kicad_sch,.kicad_pcb,.kicad_pro}`,
KiCad 10.0.7 `kicad-cli`. Test oracle: `tools/kicad-validation/kicad_validate.py`
(fingerprint ledger `validation-baseline.json`) + `pcb drc` / `sch erc` /
`sch export netlist` diffs. All edits scripted s-expression rewrites, run
against working copies; only the verified result touches the repo.

## 2. Mapping table (authoritative, evidence in spec)

| Board ref (before) | Board ref (after) | Basis |
|---|---|---|
| `LOADCELL` | `J4` | footprint JST_XH_B4B + net `Net-(HX1-A+)` on J4.1 |
| `ENDSTOP1` | `J5` or `J6` | net membership vs sch J5/J6 pins (T002 resolves) |
| `ENDSTOP2` | the other of `J5`/`J6` | ditto |
| `HX711` | `HX1` | footprint `HX711-Breakout-6-4` unique |
| `TMC2209` (upper) | `TMC1`/`TMC2` | net membership vs sch TMC1/TMC2 pins (T002 resolves) |
| `TMC2209` (lower) | the other | ditto |
| `C1 // 100µF` ... `C6 // 100nF`, `D1 // 1N5819`, `F1 // Fuse 2A T`, `U1 // TSR_1-2450` | bare `C1`..`C6`, `D1`, `F1`, `U1` | FR-002 |
| `REF**` x4 | `H1`..`H4` | FR-003, position order TL, TR, BL, BR |

## 3. Method

1. Work on a scratch copy of the three project files (never edit the repo file
   in place before gates pass).
2. Pairing resolution script: for sch J5/J6 and TMC1/TMC2, collect their pin
   net-name sets from `sch export netlist`; for board ENDSTOPx / TMC2209
   footprints, collect pad net names from the board; match by exact set
   equality. Ambiguity or no-match -> stop + escalate (spec limitation).
3. Rewrite pass on the board s-expr, anchored edits only:
   `(property "Reference" "<old>"` inside the specific footprint block ->
   new value (block-scoped, never global text replace); mounting holes get
   `(attr exclude_from_bom exclude_from_pos_files)` added to their footprint
   header line.
4. Netlist invariant: export netlist from fixed sch (unchanged) is identical
   trivially; the real check is connectivity: diff old/new board net->pin
   multisets with refs mapped through the table (script, must be equal).
5. Ledger: run `kicad_validate.py --hw <scratch>` against the copied baseline
   -> EXPECT RED exactly on `parity` + `gerber_sha`, ok elsewhere. Then copy
   into repo, commit fix; `--update` re-baseline in a separate commit; plain
   run GREEN.
6. README known-issues: mark annotation debt fixed + rev-1 silk caveat.

## 4. Risks

- KiCad parity may still report the exclude-flagged mounting holes ->
  success criterion allows <= 4 mount-hole-only findings (measured T005).
- A ref property also appears in `(net ...)` name tokens on the board? No -
  board net names are copied strings like `Net-(J5-...)`; renames must NOT
  rewrite net names (electrical identity stays). Anchored property-only edits
  guarantee this.
