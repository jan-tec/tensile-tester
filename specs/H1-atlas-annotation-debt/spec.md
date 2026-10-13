# Feature Specification: H1 - ATLAS Annotation Debt Fix

Status: Ready (dialog 2026-10-13) | Rigor: Spec-Anchored | Created: 2026-10-13 | Depends on: tools/kicad-validation ledger (#1279 dogfood)

**Goal:** Eliminate the reference-annotation debt between
`hardware/mainboard-atlas/` schematic and board so the schematic-parity
fingerprint drops from 44 findings to zero, without touching electrical
content, and re-baseline the fingerprint ledger deliberately.

**Decision record (designer dialog, 2026-10-13):**

| Decision | Ruling |
|----------|--------|
| Naming authority | **Schematic owns references** (KiCad standard). The board adopts schematic refs. |
| Mounting holes | Annotate `H1..H4`, set exclude flags so BOM stays clean. |
| Fab baseline | Source-only fix; `validation-baseline.json` re-baselined; frozen `gerber/` package stays as the built rev-1 (corrupt silks remain on the physical boards). |

## User Scenarios

### US1 (P1) - One canonical reference per component

As a maintainer, every component carries the same reference in schematic,
board and BOM, so netlists, BOMs and bug reports agree.

- Given the fixed board, When `kicad_validate.py` runs, Then parity reports 0 findings.
- Given the fixed board, When the BOM is exported, Then no reference contains `//` and no reference is `REF**`.

### US2 (P1) - No regeneration surprise

As a designer, the fix must not alter any electrical or geometric decision.

- Given the pre-fix DRC fingerprint (0 errors / 2 warnings / 0 unconnected),
  When the fix is applied, Then DRC and ERC results are unchanged
  (the 2 courtyard warnings may cite renamed refs in their messages only).
- Given netlists before and after, When compared, Then net names and
  net->pin membership are byte-identical (renames only touch `ref` display
  fields in the netlist, never the connectivity).

### US3 (P2) - Ledger discipline demonstrated

As a reviewer, the whole change is proven by the fingerprint ledger: one
explained drift, one deliberate re-baseline.

- Given the pre-fix baseline, When the fix is applied, Then the ledger is RED
  on exactly `gerber_sha` and `parity` (expected drift) and ok on `drc`,
  `erc`, `counts`.
- After re-baseline, the ledger is GREEN and the diff to the old baseline
  shows only the two expected fields.

## Requirements

### Functional

- **FR-001:** Board reference renames (board adopts schematic):
  `LOADCELL` -> `J4`, `HX711` -> `HX1`, `TMC2209` x2 -> `TMC1`/`TMC2`,
  `ENDSTOP1`/`ENDSTOP2` -> `J5`/`J6`. TMC and ENDSTOP pairing is decided by
  net-list comparison (footprints are identical, so position must not decide),
  documented in the commit.
- **FR-002:** Strip `// <value>` corruption from board reference fields:
  `C1 // 100µF`..`C6 // 100nF`, `D1 // 1N5819`, `F1 // Fuse 2A T`,
  `U1 // TSR_1-2450` -> bare refs. The reference field is the silkscreen
  text, so the fix also cleans future spins' silk.
- **FR-003:** The four `REF**` mounting holes (MountingHole_3.2mm_M3) become
  `H1..H4` (order: top-left, top-right, bottom-left, bottom-right) with
  `exclude_from_bom` and `exclude_from_pos_files` attributes set.
- **FR-004:** Electrical content is frozen: no net, track, pad, zone, via,
  layer or footprint-position change; `counts` fingerprint items
  (footprints/pads/uuid tokens/format) stay byte-identical.
- **FR-005:** `validation-baseline.json` is re-baselined once after the fix
  (deliberate act, separate commit) and the ledger then passes GREEN.
- **FR-006:** `gerber/` and `img/` are untouched; README known-issues ledger
  is updated to record the fix and the as-built rev-1 caveat.

### Constraints & TDD Targets

- Tooling: scripted s-expression edits only (headless, reproducible); the
  kicad-cli validation gate is the test oracle - every task states its
  expected ledger/DRC delta before execution (EXPECT-before-run discipline).
- Schematic is not edited at all (it is already the naming authority).
- KiCad 10.0.7 / `kicad-cli` only for validation; no GUI steps.

### Non-Functional

- Every commit < 200 LOC diff excluding the mechanical property-text rewrite
  of the board file (the rename rewrite is one mechanical concern).

## Success Criteria

1. `kicad_validate.py` GREEN against the new baseline; `parity == 0`
   (or only mount-hole findings if KiCad 10 parity ignores exclude flags -
   measured at implementation, max 4, all mounting-hole).
2. DRC fingerprint unchanged: 0 errors / 2 warnings / 0 unconnected.
3. Netlist pin->net membership identical before/after (scripted diff).
4. Ledger shows exactly 2 drifted fields between old and new baseline
   (`parity`, `gerber_sha`); `counts` and `drc` identical.
5. `git diff` on the schematic is empty.

## Assumptions / Limitations

- The physical rev-1 boards keep their corrupt silks (`C6 // 100nF`);
  only the next fab spin prints clean references. Accepted by designer.
- `bom_sha` is exported from the schematic, which is unchanged -> expected
  stable; if it moves, that itself is a parity clue and must be explained.
- The ENDSTOP1/ENDSTOP2 -> J5/J6 and TMC2209 x2 -> TMC1/TMC2 pairing is
  derived mechanically from net names; if a pairing cannot be resolved
  unambiguously, the fix stops and escalates instead of guessing.
