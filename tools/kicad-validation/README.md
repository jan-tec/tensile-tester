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
kicad-venv/bin/python tools/kicad-validation/kicad_validate.py
```

Green = baseline DRC has 0 errors / 0 unconnected AND the kicad-tools
round-trip is DRC-identical. Run it after any agent-driven board edit.

## IPC (live GUI) notes

- Enable once: Settings → `kicad_common.json` → `api.enable_server: true`.
- Each GUI/pcbnew instance serves its own socket `/tmp/kicad/api[-<pid>].sock`.
- Connect: `KiCadClient("ipc:///tmp/kicad/api-<pid>.sock", "name", "", 30000)`;
  empty token is accepted by KiCad 10.
- Gotchas (0.7.1): refs via `f.reference_field.text.value`, positions in
  microns via `f.position.x/.y`, `k.get_board()` takes no arg (active board).
