#!/usr/bin/env python3
"""M2 locale-drift gate (#89). Runs in the CI `test` job, and locally as part
of the pre-PR gate. Stdlib only.

Two drift windows around src/Text.h (found in the #87 review):

1. STALE ROWS - a Msg id in the X-macro list that no call site references
   anymore (translation text kept without a surface using it). Every id must
   be referenced by `Msg::ID` in some src/ file outside the X-list itself
   (the msgForFault map inside Text.h counts: it is a live presentation map).

2. RAW USER-FACING LITERALS - user-facing text must route through `msg(Msg::)`
   (SC-003). Any raw string literal passed to Serial.print/println or
   jsonMessage() fails, except the explicit allowlists below:
   - src/Diagnostics.cpp: the BOOT/CHECK/STATE log grammar is machine ops
     text, not locale UI (M2 plan D6 keeps it English, stable).
   - `json +=` statements: /api/status keys and machine-readable field values
     (SSID/IP tokens) are API contract, localized labels live in the JS dict.

Exit 0 = clean; exit 1 with a per-finding report otherwise.
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
TEXT_H = ROOT / "src" / "Text.h"
SRC = ROOT / "src"

# Files whose Serial.print string literals are allowlisted (documented above).
PRINT_LITERAL_ALLOWLIST_FILES = {"Diagnostics.cpp"}

LIT = r'"(?:[^"\\]|\\.)*"'
MSG_ID_RE = re.compile(r"^  MSG\((\w+),", re.M)


def parse_msg_ids(text_h: str) -> list[str]:
    return MSG_ID_RE.findall(text_h)


def stale_ids(ids: list[str], text_h: str) -> list[str]:
    # Everything in src/ that is not the X-list: other files, plus the Text.h
    # tail after the list (msgForFault etc.).
    haystacks = [
        (p.name, p.read_text())
        for p in sorted(SRC.glob("*"))
        if p.suffix in {".h", ".cpp"} and p != TEXT_H
    ]
    tail = text_h[text_h.index("#undef MSG_X_LIST") :]
    stale = []
    for mid in ids:
        pattern = re.compile(rf"\bMsg::{mid}\b")
        used = bool(pattern.search(tail)) or any(
            pattern.search(content) for _, content in haystacks
        )
        if not used:
            stale.append(mid)
    return stale


def raw_print_literals() -> list[str]:
    """flag every raw string literal argument of Serial.print/println or
    jsonMessage() outside the allowlisted files."""
    findings = []
    stmt_re = re.compile(r"(Serial\.(?:print|println)|jsonMessage)\s*\(")
    for path in sorted(SRC.glob("*.cpp")):
        if path.name in PRINT_LITERAL_ALLOWLIST_FILES:
            continue
        text = path.read_text()
        # Join physical lines into logical statements (these call sites never
        # span more than a few lines; stop at the first ';' after the open).
        for m in stmt_re.finditer(text):
            start = m.start()
            end = text.find(";", m.end())
            if end == -1:
                continue
            stmt = " ".join(text[start:end].split())
            # A `{` means we walked into a function body: this was the
            # definition of jsonMessage, not a call.
            if "{" in stmt:
                continue
            if re.search(LIT, stmt):
                findings.append(f"{path.name}: {stmt[:100]}")
    return findings


def main() -> int:
    text_h = TEXT_H.read_text()
    ids = parse_msg_ids(text_h)
    if not ids:
        print("FATAL: no MSG() rows found in src/Text.h - parser drift?")
        return 1

    failures = []

    stale = stale_ids(ids, text_h)
    for mid in stale:
        failures.append(f"stale Msg row (no call site references it): Msg::{mid}")

    for finding in raw_print_literals():
        failures.append(f"raw user-facing literal (must use msg(Msg::...)): {finding}")

    if failures:
        print("check_locale_coverage: FAILED")
        for f in failures:
            print(f"  - {f}")
        return 1
    print(f"check_locale_coverage: OK ({len(ids)} Msg ids in use, no raw literals)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
