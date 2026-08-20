#!/usr/bin/env python3
"""CI layout lock: HemolymphBar required, LifeBar10/11 names forbidden.

Dylan's live pack (byte-identical vanilla) has LifeBar1-9 only and
MedicalPanel y=0.712963. That unbound LifeBar10 slot is the class
Kenshi survives. Our override must keep Dark UI look: HemolymphBar
at the current Root-after-MedicalPanel coords, MedicalPanel y locked
to 0.70473849555969238. Do not rebase onto vanilla 1-9 / 0.712963.
"""
from pathlib import Path
import sys

p = Path(__file__).resolve().parents[1] / "kenshi_mod" / "gui" / "layout" / "Kenshi_MainPanel.layout"
raw = p.read_bytes()
if not raw.startswith(b"\xef\xbb\xbf"):
    print("FAIL: layout missing UTF-8 BOM", file=sys.stderr)
    sys.exit(1)
text = raw.decode("utf-8-sig")
if 'name="HemolymphBar"' not in text:
    print("FAIL: name=\"HemolymphBar\" missing", file=sys.stderr)
    sys.exit(1)
# Occupying Kenshi's LifeBar10 assignWidget slot is the H2 bug.
banned = []
for token in (
    'name="LifeBar10"',
    'name="LifeBar10Datapanel"',
    'name="LifeBar10Value"',
    'name="LifeBar10Tooltip"',
    'name="LifeBar10Green"',
    'name="LifeBar10Grey"',
    'name="LifeBar10Red"',
    'name="LifeBar10Yellow"',
    'name="LifeBar10White"',
    'name="LifeBar10Robot"',
    'name="LifeBar10Crushed"',
    'name="LifeBar11"',
    'name="LifeBar11Value"',
    'name="LifeBar11Tooltip"',
):
    if token in text:
        banned.append(token)
if banned:
    print("FAIL: occupied Kenshi bind names:", ", ".join(banned), file=sys.stderr)
    sys.exit(1)
if "LifeBar10Slot" in text or "HemolymphStrip" in text:
    print("FAIL: rejected chrome wrapper present", file=sys.stderr)
    sys.exit(1)
if "0.70473849555969238" not in text:
    print("FAIL: MedicalPanel y drifted", file=sys.stderr)
    sys.exit(1)
print("layout ok: HemolymphBar present, LifeBar10/11 names absent")
