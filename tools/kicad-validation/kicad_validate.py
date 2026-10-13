#!/usr/bin/env python3
"""KiCad agentic-AI validation gate for Mainboard-ATLAS.

Run: /tmp/opencode/tensile-tester/kicad-venv/bin/python tools/kicad-validation/kicad_validate.py
Expects env: kicad-cli on PATH (KiCad >= 10.0.7).

Checks:
  1. kicad-tools (kicad_tools 0.22+) round-trip of the board is DRC-identical
     to kicad-cli DRC on the original file (the safe headless edit layer).
  2. Baseline DRC of the tracked board: errors must be 0 and unconnected 0.

Exit codes: 0 = all green, 1 = round-trip drift or DRC regression.
"""
import json
import subprocess
import sys
import tempfile
from pathlib import Path

HW = Path(__file__).resolve().parents[2] / "hardware" / "mainboard-atlas"
PCB = HW / "Mainboard-ATLAS.kicad_pcb"
CONTEXT = ["Mainboard-ATLAS.kicad_pro", "sym-lib-table", "fp-lib-table",
           "Breakoutboards.kicad_sym", "PEM.kicad_sym"]


def drc(pcb: Path, workdir: Path) -> dict:
    out = workdir / "drc.json"
    r = subprocess.run(
        ["kicad-cli", "pcb", "drc", "--severity-all", "--format", "json",
         "--output", str(out), str(pcb)],
        capture_output=True, text=True)
    if r.returncode != 0 or not out.exists():
        sys.exit(f"kicad-cli DRC failed (exit {r.returncode}): {r.stderr[-200:]}")
    d = json.loads(out.read_text())
    return {
        "errors": sum(1 for v in d["violations"] if v["severity"] == "error"),
        "warnings": sum(1 for v in d["violations"] if v["severity"] == "warning"),
        "unconnected": len(d["unconnected_items"]),
    }


def main() -> int:
    tmp = Path(tempfile.mkdtemp(prefix="kicad-validate-"))
    for f in CONTEXT:  # libs/rules context so DRC is comparable
        src = HW / f
        if src.is_dir():
            import shutil
            shutil.copytree(src, tmp / f)
        elif src.exists():
            (tmp / f).write_bytes(src.read_bytes())
    board = tmp / PCB.name
    board.write_bytes(PCB.read_bytes())

    baseline = drc(board, tmp)
    print("baseline DRC:", baseline)
    ok = baseline["errors"] == 0 and baseline["unconnected"] == 0

    from kicad_tools import load_pcb, save_pcb  # kicad-tools >= 0.22
    rt = tmp / "roundtrip.kicad_pcb"
    save_pcb(load_pcb(str(board)), str(rt))
    after = drc(rt, tmp)
    print("kicad-tools round-trip DRC:", after)
    ok &= after == baseline

    if ok:
        print("KICAD VALIDATE: GREEN")
        return 0
    print("KICAD VALIDATE: RED")
    return 1


if __name__ == "__main__":
    sys.exit(main())
