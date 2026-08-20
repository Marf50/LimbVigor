#!/usr/bin/env python3
"""CI layout lock: HemolymphBar required, LifeBar10/11 names forbidden.

H2-name is falsified. HemolymphBar must be last child of already-grown
MedicalPanel_Back after LifeBar9 (locked leftover-pad coords). Value and
Tooltip on Front after LifeBar9*. No Hemolymph* on Root. MedicalPanel y
stays 0.70473849555969238. Do not grow Back/Front. Do not rebase onto
vanilla 1-9 / 0.712963.
"""
from pathlib import Path
import re
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
if 'position_real="0.040133778005838394 0.24074074625968933 0.92976588010787964 0.7469136118888855" name="MedicalPanel_Back"' not in text:
    print("FAIL: MedicalPanel_Back grown or drifted", file=sys.stderr)
    sys.exit(1)
if 'position_real="0 0 0.99665552377700806 1.0030864477157593" name="MedicalPanel_Front"' not in text:
    print("FAIL: MedicalPanel_Front grown or drifted", file=sys.stderr)
    sys.exit(1)
hemo = 'position_real="0.0071942447684705257 0.9008264392614365 0.85611510276794434 0.095041319727897644" name="HemolymphBar"'
if hemo not in text:
    print("FAIL: HemolymphBar locked leftover-pad coords missing", file=sys.stderr)
    sys.exit(1)
if 'position_real="0.84563755989074707 0.92307692766189575 0.12751677632331848 0.052307691425085068" name="HemolymphValue"' not in text:
    print("FAIL: HemolymphValue locked Front coords missing", file=sys.stderr)
    sys.exit(1)
if 'position_real="0.026845637708902359 0.91692310571670532 0.94295299053192139 0.064615383744239807" name="HemolymphTooltip"' not in text:
    print("FAIL: HemolymphTooltip locked Front coords missing", file=sys.stderr)
    sys.exit(1)
if "0.14218749750845783 0.9797385139738242" in text:
    print("FAIL: old Root HemolymphBar coords remain", file=sys.stderr)
    sys.exit(1)
idx9 = text.find('name="LifeBar9"')
idxh = text.find('name="HemolymphBar"')
idxfront = text.find('name="MedicalPanel_Front"')
if not (0 <= idx9 < idxh < idxfront):
    print("FAIL: HemolymphBar is not after LifeBar9 inside Back", file=sys.stderr)
    sys.exit(1)
root_hemo = re.findall(r'\n        <Widget[^>]*name="(Hemolymph[^"]+)"', text)
if root_hemo:
    print("FAIL: Hemolymph* still Root siblings:", ", ".join(root_hemo), file=sys.stderr)
    sys.exit(1)
print("layout ok: HemolymphBar last child of Back after LifeBar9, LifeBar10/11 names absent")
