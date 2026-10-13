# Tasks: H1 - ATLAS Annotation Debt Fix

Gate: every step runs on a scratch copy; `kicad_validate.py` + netlist
invariant green before anything lands in the repo; ledger drift must match
the EXPECT line exactly (expect-before-run). 1 task = 1 commit here (single
branch `test/kicad10`, no PR per task - this is the agentic testing ground).

^- [x] T001 [EXPECT: pairing script prints unique J5/J6 + TMC1/TMC2 matches] net-membership pairing resolver from `sch export netlist` + board pad nets (plan §2)
^- [x] T002 [EXPECT: unambiguous] apply full rename map + `//`-suffix strip + H1..H4 with exclude attrs to scratch board s-expr (block-anchored property edits only)
^- [x] T003 [EXPECT: net->pin multisets equal under mapping] connectivity invariant diff old vs new board
^- [x] T004 [EXPECT: ledger RED on exactly parity+gerber_sha, ok drc/erc/counts; DRC still 0/2/0] run `kicad_validate.py --hw <scratch>` vs copied baseline + negative-check `sch` diff empty
^- [x] T005 [EXPECT: parity == 0, or mount-hole-only findings <= 4 - record which] measure parity findings composition after fix
^- [x] T006 land fixed board in repo (commit 1: the fix)
^- [x] T007 `kicad_validate.py --update` re-baseline + GREEN re-run (commit 2: deliberate re-baseline, message cites spec H1)
^- [x] T008 README known-issues: annotation debt -> fixed (source-level), rev-1 silk caveat; ledger baseline note updated (commit 3)
