# Mainboard ATLAS - CAD sources

Custom carrier mainboard of the tensile test machine: hosts the ESP32
DevKit (2x15 pin header footprint), HX711 load-cell breakout, TMC2209
stepper driver breakouts, endstop connectors and the 5 V rail.

Imported 2026-10-10 from the designer's KiCad project (folder `raw/`,
untracked) into version control. KiCad 9 project.

## Files

| File | Purpose |
|------|---------|
| `Mainboard-ATLAS.kicad_sch` | Main schematic (ENDSTOP1/ENDSTOP2 nets, power, HX711, drivers) |
| `Mainboard-ATLAS.kicad_pcb` | PCB layout (2-layer) |
| `Mainboard-ATLAS.kicad_pro` / `.kicad_prl` | KiCad project / board settings |
| `Breakoutboards.kicad_sym` | Custom symbols (ESP32 DevKit, HX711 carrier) |
| `PEM.kicad_sym` | PEM fastener symbols |
| `Breakoutboard-Footprints.pretty/` | Custom footprints |
| `fp-lib-table`, `sym-lib-table` | Library tables (use `${KIPRJMOD}` relative refs) |
| `gerber/` | Manufacturing output of record (2026-07-09 fab run) |

## Dropped on import (junk)

`.DS_Store`, KiCad lock files (`~*.lck`), `fp-info-cache` (4 MB cache,
regenerates), `Breakoutboards.bak`, `Mainboard-ATLAS-backups/` (28
auto-backup zips), `Experiment.kicad_pcb.zip` (predecessor board,
Jan 2026, superseded by Mainboard-ATLAS), `gerber.zip` (byte-identical
duplicate of `gerber/` plus `__MACOSX/` AppleDouble junk - content
cmp-verified equal during PR review).

## CLI usage (no GUI needed)

```bash
# netlist (net -> pin mapping, e.g. ENDSTOP1):
kicad-cli sch export netlist --format kicadsexpr \
  hardware/mainboard-atlas/Mainboard-ATLAS.kicad_sch -o /tmp/atlas.net
# render schematic/PCB to PNG/SVG/PDF:
kicad-cli sch export svg Mainboard-ATLAS.kicad_sch -o out/
kicad-cli pcb export svg Mainboard-ATLAS.kicad_pcb -o out/ --page-size-mode current
```

Python parsing/writing of these files: `kiutils` (see repo `.venv`).
