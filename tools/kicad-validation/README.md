# KiCad Agentic-AI Validation (test/kicad10)

Toolchain validated 2026-10-13 against `Mainboard-ATLAS` with KiCad 10.0.7
(CachyOS packages). Board re-saved to format 20260206 (commit a2dc5aa).

| Layer | Tool | Status on KiCad 10.0.7 |
|-------|------|------------------------|
| Validate/export | `kicad-cli` | ✅ DRC/ERC headless, the authority |
| Headless parse/edit | `kicad-tools` 0.22 (`load_pcb`/`save_pcb`) | ✅ round-trip lossless, DRC-identical |
| Live GUI inspect | `kicad-python==0.7.1` (module `kipy`) over IPC | ✅ read path (footprints/nets/positions); ⚠️ no geometry mutation on KiCad 10 |
| File-based mutation | s-expr edit via `kicad-tools` + `kicad-cli` gate | ✅ only mutation loop available headless today |
| kiutils 1.4.8 | - | ❌ cannot parse KiCad 10 format (`(net "NAME")` shorthand); do not use |
| kicad-python 0.8.0 | - | ❌ speaks KiCad 11 API (`kicad-cli api-server` = KiCad 11+) |
| Real-time IPC mutation | needs KiCad 11 upgrade | ⏳ |

## Setup

```bash
uv venv kicad-venv --python 12
uv pip install --python kicad-venv/bin/python kicad-tools "kicad-python==0.7.1"
```

## Gate

```bash
python3 tools/kicad-validation/kicad_validate.py            # compare vs baseline
python3 tools/kicad-validation/kicad_validate.py --update   # deliberate re-baseline (new spin only)
```

Fingerprint ledger (`hardware/mainboard-atlas/validation-baseline.json`):
DRC/ERC/parity counts, s-expr token counts, BOM hash, and a geometry-only
Gerber replot hash (date/version lines and `.gbrjob` stripped - verified
deterministic across runs). Any RED line must be explained by the edit that
caused it; the gate was negative-control tested (mutant track -> RED).
Needs only `kicad-cli` + `python3` (verified reproducible from a clean clone;
CI-container usage not yet tried). Exit codes: 0 green, 1 drift, 2 tooling
failure.

## IPC (live GUI) notes

- Enable once: Settings → `kicad_common.json` → `api.enable_server: true`.
- Each GUI/pcbnew instance serves its own socket `/tmp/kicad/api[-<pid>].sock`.
- Connect: `KiCadClient("ipc:///tmp/kicad/api-<pid>.sock", "name", "", 30000)`;
  empty token is accepted by KiCad 10.
- Gotchas (0.7.1): refs via `f.reference_field.text.value`, positions in
  microns via `f.position.x/.y`, `k.get_board()` takes no arg (active board).
