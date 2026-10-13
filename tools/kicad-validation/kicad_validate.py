#!/usr/bin/env python3
"""KiCad fingerprint ledger gate for Mainboard-ATLAS (test/kicad10).

Every agent-driven board or schematic edit must run this gate. It compares a
fingerprint ledger (validation-baseline.json) against the current tree:

  drc        kicad-cli pcb drc        errors/warnings/unconnected
  erc        kicad-cli sch erc        violation count
  parity     kicad-cli pcb drc --schematic-parity   finding count
  counts     s-expr token counts      footprints / pads / uuid tokens / format
  bom        kicad-cli sch export bom  sha256 of grouped Reference,Value
  gerber     replot of the fab layer set + drill, sha256 over geometry
             (TF.GenerationSoftware / TF.CreationDate / G04 / ';' stripped)

Usage:
  kicad_validate.py             # compare tree against the baseline; RED on drift
  kicad_validate.py --update    # re-baseline (only for deliberate re-spins)

Exit codes: 0 green, 1 drift detected, 2 tooling failure.
"""
import argparse
import hashlib
import json
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
DEFAULT_HW = REPO / "hardware" / "mainboard-atlas"
FAB_LAYERS = "F.Cu,B.Cu,F.Mask,B.Mask,F.Silkscreen,B.Silkscreen,Edge.Cuts"  # comma-separated; Edge.Cuts (dot) is the CLI token


def cli(*args) -> subprocess.CompletedProcess:
    return subprocess.run(["kicad-cli", *args], capture_output=True, text=True)


def run(cmd: list[str]) -> subprocess.CompletedProcess:
    return subprocess.run(cmd, capture_output=True, text=True)


def drc_summary(pcb: Path, workdir: Path) -> dict:
    out = workdir / "drc.json"
    r = cli("pcb", "drc", "--severity-all", "--format", "json", "--output", str(out), str(pcb))
    if r.returncode != 0 or not out.exists():
        sys.exit(f"tooling failure: pcb drc exit={r.returncode} {r.stderr[-200:]}")
    d = json.loads(out.read_text())
    return {"errors": sum(1 for v in d["violations"] if v["severity"] == "error"),
            "warnings": sum(1 for v in d["violations"] if v["severity"] == "warning"),
            "unconnected": len(d["unconnected_items"])}


def parity_count(pcb: Path, workdir: Path) -> int:
    out = workdir / "parity.json"
    r = cli("pcb", "drc", "--schematic-parity", "--format", "json", "--output", str(out), str(pcb))
    if r.returncode != 0 or not out.exists():
        sys.exit(f"tooling failure: parity exit={r.returncode}")
    return len(json.loads(out.read_text())["schematic_parity"])


def erc_count(sch: Path, workdir: Path) -> int:
    out = workdir / "erc.json"
    r = cli("sch", "erc", "--format", "json", "--output", str(out), str(sch))
    if r.returncode != 0 or not out.exists():
        sys.exit(f"tooling failure: erc exit={r.returncode}")
    return len(json.loads(out.read_text())["sheets"][0].get("violations", []))


def counts(pcb: Path) -> dict:
    txt = pcb.read_text()
    return {"footprints": txt.count("(footprint "), "pads": txt.count("(pad "),
            "uuid_tokens": txt.count("(uuid "),
            "format": re.search(r"\(version (\d+)\)", txt).group(1)}


def sha(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def bom_sha(sch: Path, workdir: Path) -> str:
    out = workdir / "bom.csv"
    r = cli("sch", "export", "bom", "--fields", "Reference,Value",
            "--group-by", "Reference", "--output", str(out), str(sch))
    if r.returncode != 0 or not out.exists():
        sys.exit(f"tooling failure: bom exit={r.returncode} {r.stderr[-200:]}")
    lines = sorted(l for l in out.read_text().splitlines() if l.strip())
    return sha("\n".join(lines).encode())


def gerber_sha(pcb: Path, workdir: Path) -> str:
    g = workdir / "plot"
    g.mkdir(exist_ok=True)
    r = cli("pcb", "export", "gerbers", "--layers", FAB_LAYERS, "--output", str(g), str(pcb))
    r2 = cli("pcb", "export", "drill", "--output", str(g / "drill"), str(pcb))
    if r.returncode != 0 or r2.returncode != 0:
        sys.exit(f"tooling failure: plot exit={r.returncode}/{r2.returncode}")
    plotted = [f for f in g.rglob("*") if f.is_file() and f.suffix != ".gbrjob"]
    if not plotted:  # empty plot set would silently hash to a constant
        sys.exit("tooling failure: gerber export produced no layer files")
    blob = []
    for f in sorted(g.rglob("*")):
        if not f.is_file() or f.suffix == ".gbrjob":  # job = metadata, not geometry
            continue
        for line in f.read_text(errors="replace").splitlines():
            s = line.strip()
            if (s.startswith("TF.GenerationSoftware") or s.startswith("TF.CreationDate")
                    or s.startswith("%TF.") or s.startswith("G04") or s.startswith(";")
                    or s.startswith("FileCreationDate") or s.startswith("CreationDate")):
                continue
            blob.append(s)
    return sha("\n".join(blob).encode())


def fingerprint(hw: Path) -> dict:
    tmp = Path(tempfile.mkdtemp(prefix="fingerprint-"))
    try:  # project context next to copies so DRC severities/nets match
        for f in hw.iterdir():
            if f.name.startswith(("gerber", "img", "validation-baseline")):
                continue
            dest = tmp / f.name
            shutil.copytree(f, dest) if f.is_dir() else shutil.copy(f, dest)
        pcb, sch = tmp / "Mainboard-ATLAS.kicad_pcb", tmp / "Mainboard-ATLAS.kicad_sch"
        return {"drc": drc_summary(pcb, tmp), "erc_violations": erc_count(sch, tmp),
                "parity": parity_count(pcb, tmp), "counts": counts(pcb),
                "bom_sha": bom_sha(sch, tmp), "gerber_sha": gerber_sha(pcb, tmp)}
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--update", action="store_true", help="re-baseline deliberately")
    ap.add_argument("--hw", type=Path, default=DEFAULT_HW, help="hardware dir override")
    a = ap.parse_args()
    baseline_path = a.hw / "validation-baseline.json"
    current = fingerprint(a.hw)

    if a.update:
        baseline_path.write_text(json.dumps(current, indent=2) + "\n")
        print(f"baseline written: {baseline_path}")
        return 0
    if not baseline_path.exists():
        print("no baseline - run with --update first")
        return 2
    base = json.loads(baseline_path.read_text())
    drift = [k for k in base if base[k] != current.get(k)]
    for k in base:
        status = "DRIFT" if k in drift else "ok"
        print(f"  {k:15s} {status:5s} {current.get(k)}")
    if drift:
        print(f"KICAD VALIDATE: RED ({', '.join(drift)} changed - every drift must be explained by the edit)")
        return 1
    print("KICAD VALIDATE: GREEN (fingerprint ledger matches baseline)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
