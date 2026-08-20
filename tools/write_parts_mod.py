#!/usr/bin/env python3
"""Write LimbVigor.mod as a type-16 FCS file with 20 LimbReplacement items.

Format matches OpenConstructionSet DataFileType.Mod + OcsWriter:
  header (type, version, author, description, dependencies, references, lastId, itemCount)
  each item: length, type, id, name, stringId, changeType=New(0),
             7 value groups (bool, float, int, vec3, vec4, string, file),
             ref-category count, instances.

LimbReplacement = 111. Slot numbers match RobotLimbs::Limb:
  LEFT_ARM=0 RIGHT_ARM=1 LEFT_LEG=2 RIGHT_LEG=3

FileValue mesh/icon are required. createItem cannot equip a mesh-less
record. Paths reuse the vanilla Economy limb visuals (same files the
game already ships). C++ still refuses a mesh-less GameData hit and
never createItem()s an Economy prosthetic.
"""
from __future__ import annotations

import argparse
import struct
from pathlib import Path

LIMB_REPLACEMENT = 111
CHANGE_NEW = 0

DESC = (
    "Organic races can grow a lost limb back. "
    "Hive: innate hemolymph. "
    "Shek: toughness 20 or a used Splint Kit. "
    "Human: toughness 40 and Field Medic 25, or a used Splint Kit. "
    "Skeletons never. "
    "Growth stages are real limb parts (I-key slot). Needs RE_Kenshi."
)

# Vanilla Economy visuals. FCS FileValue keys for LIMB_REPLACEMENT:
#   mesh, mesh female, icon
# Paths are the game-root form FCS writes when you pick a file under data/.
MESH_BY_LIMB = {
    "r-leg": r".\data\items\robotics\economy leg R.mesh",
    "l-leg": r".\data\items\robotics\economy leg L.mesh",
    "r-arm": r".\data\items\robotics\economy arm R.mesh",
    "l-arm": r".\data\items\robotics\economy arm L.mesh",
}
ICON_BY_LIMB = {
    "r-leg": r".\data\items\robotics\economy leg R.png",
    "l-leg": r".\data\items\robotics\economy leg L.png",
    "r-arm": r".\data\items\robotics\economy arm R.png",
    "l-arm": r".\data\items\robotics\economy arm L.png",
}

DESC_BY_STAGE = {
    "stump": "A raw stump. Almost no push-off.",
    "bud": "Flesh is budding on the stump.",
    "form": "Bone and tendon are finding their shape.",
    "knit": "Almost a limb. Soft. Do not test it.",
    "grown": "A new limb. Soft. Yours.",
}

# (limb_key, stage, name, string_id, desc, slot, hp, ath, stl, swim, dex, stren, combat, thievery, weight)
PARTS = [
    ("r-leg", "stump", "LV Stump Right Leg", "lv-stump-r-leg",
     DESC_BY_STAGE["stump"], 3, 30, 0.15, 0.20, 0.10, 1.0, 1.0, 1.0, 1.0, 0.4),
    ("r-leg", "bud", "LV Budding Right Leg", "lv-bud-r-leg",
     DESC_BY_STAGE["bud"], 3, 45, 0.35, 0.40, 0.25, 1.0, 1.0, 1.0, 1.0, 0.8),
    ("r-leg", "form", "LV Forming Right Leg", "lv-form-r-leg",
     DESC_BY_STAGE["form"], 3, 65, 0.60, 0.65, 0.50, 1.0, 1.0, 1.0, 1.0, 1.4),
    ("r-leg", "knit", "LV Knitting Right Leg", "lv-knit-r-leg",
     DESC_BY_STAGE["knit"], 3, 85, 0.85, 0.85, 0.75, 1.0, 1.0, 1.0, 1.0, 2.0),
    ("r-leg", "grown", "LV Grown Right Leg", "lv-grown-r-leg",
     DESC_BY_STAGE["grown"], 3, 100, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 2.4),
    ("l-leg", "stump", "LV Stump Left Leg", "lv-stump-l-leg",
     DESC_BY_STAGE["stump"], 2, 30, 0.15, 0.20, 0.10, 1.0, 1.0, 1.0, 1.0, 0.4),
    ("l-leg", "bud", "LV Budding Left Leg", "lv-bud-l-leg",
     DESC_BY_STAGE["bud"], 2, 45, 0.35, 0.40, 0.25, 1.0, 1.0, 1.0, 1.0, 0.8),
    ("l-leg", "form", "LV Forming Left Leg", "lv-form-l-leg",
     DESC_BY_STAGE["form"], 2, 65, 0.60, 0.65, 0.50, 1.0, 1.0, 1.0, 1.0, 1.4),
    ("l-leg", "knit", "LV Knitting Left Leg", "lv-knit-l-leg",
     DESC_BY_STAGE["knit"], 2, 85, 0.85, 0.85, 0.75, 1.0, 1.0, 1.0, 1.0, 2.0),
    ("l-leg", "grown", "LV Grown Left Leg", "lv-grown-l-leg",
     DESC_BY_STAGE["grown"], 2, 100, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 2.4),
    ("r-arm", "stump", "LV Stump Right Arm", "lv-stump-r-arm",
     DESC_BY_STAGE["stump"], 1, 30, 1.0, 1.0, 0.10, 0.15, 0.20, 0.15, 0.10, 0.3),
    ("r-arm", "bud", "LV Budding Right Arm", "lv-bud-r-arm",
     DESC_BY_STAGE["bud"], 1, 45, 1.0, 1.0, 0.25, 0.35, 0.40, 0.30, 0.25, 0.6),
    ("r-arm", "form", "LV Forming Right Arm", "lv-form-r-arm",
     DESC_BY_STAGE["form"], 1, 65, 1.0, 1.0, 0.50, 0.60, 0.70, 0.55, 0.50, 1.1),
    ("r-arm", "knit", "LV Knitting Right Arm", "lv-knit-r-arm",
     DESC_BY_STAGE["knit"], 1, 85, 1.0, 1.0, 0.75, 0.85, 0.90, 0.80, 0.80, 1.6),
    ("r-arm", "grown", "LV Grown Right Arm", "lv-grown-r-arm",
     DESC_BY_STAGE["grown"], 1, 100, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.9),
    ("l-arm", "stump", "LV Stump Left Arm", "lv-stump-l-arm",
     DESC_BY_STAGE["stump"], 0, 30, 1.0, 1.0, 0.10, 0.15, 0.20, 0.15, 0.10, 0.3),
    ("l-arm", "bud", "LV Budding Left Arm", "lv-bud-l-arm",
     DESC_BY_STAGE["bud"], 0, 45, 1.0, 1.0, 0.25, 0.35, 0.40, 0.30, 0.25, 0.6),
    ("l-arm", "form", "LV Forming Left Arm", "lv-form-l-arm",
     DESC_BY_STAGE["form"], 0, 65, 1.0, 1.0, 0.50, 0.60, 0.70, 0.55, 0.50, 1.1),
    ("l-arm", "knit", "LV Knitting Left Arm", "lv-knit-l-arm",
     DESC_BY_STAGE["knit"], 0, 85, 1.0, 1.0, 0.75, 0.85, 0.90, 0.80, 0.80, 1.6),
    ("l-arm", "grown", "LV Grown Left Arm", "lv-grown-l-arm",
     DESC_BY_STAGE["grown"], 0, 100, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.9),
]


def put_i32(buf: bytearray, value: int) -> None:
    buf.extend(struct.pack("<i", value))


def put_u32(buf: bytearray, value: int) -> None:
    buf.extend(struct.pack("<I", value))


def put_f32(buf: bytearray, value: float) -> None:
    buf.extend(struct.pack("<f", float(value)))


def put_str(buf: bytearray, text: str) -> None:
    data = text.encode("utf-8")
    put_i32(buf, len(data))
    buf.extend(data)


def put_pair_f(buf: bytearray, key: str, value: float) -> None:
    put_str(buf, key)
    put_f32(buf, value)


def put_pair_i(buf: bytearray, key: str, value: int) -> None:
    put_str(buf, key)
    put_i32(buf, value)


def put_pair_s(buf: bytearray, key: str, value: str) -> None:
    put_str(buf, key)
    put_str(buf, value)


def put_pair_b(buf: bytearray, key: str, value: bool) -> None:
    put_str(buf, key)
    buf.append(1 if value else 0)


def write_item(part: tuple, item_id: int) -> bytes:
    (
        limb, _stage, name, sid, desc, slot, hp,
        ath, stl, swim, dex, stren, combat, thievery, weight,
    ) = part
    mesh = MESH_BY_LIMB[limb]
    icon = ICON_BY_LIMB[limb]
    if not mesh or not icon or ".mesh" not in mesh:
        raise SystemExit(f"{sid} missing mesh/icon FileValue")

    body = bytearray()
    put_i32(body, LIMB_REPLACEMENT)
    put_i32(body, item_id)
    put_str(body, name)
    put_str(body, sid)
    put_u32(body, CHANGE_NEW)

    # bools — auto icon from the male mesh if the png path is wrong
    bools = [("auto icon image", True)]
    bools.sort(key=lambda x: x[0])
    put_i32(body, len(bools))
    for k, v in bools:
        put_pair_b(body, k, v)

    # floats — keys alphabetical (OCS writer sorts)
    floats = [
        ("athletics mult", ath),
        ("athletics mult 1", ath),
        ("dexterity mult", dex),
        ("dexterity mult 1", dex),
        ("overall mult", 1.0),
        ("ranged mult", combat),
        ("ranged mult 1", combat),
        ("stealth mult", stl),
        ("stealth mult 1", stl),
        ("strength mult", stren),
        ("strength mult 1", stren),
        ("swimming mult", swim),
        ("swimming mult 1", swim),
        ("thievery mult", thievery),
        ("thievery mult 1", thievery),
        ("weight kg", weight),
    ]
    floats.sort(key=lambda x: x[0])
    put_i32(body, len(floats))
    for k, v in floats:
        put_pair_f(body, k, v)

    # ints
    ints = [
        ("HP", int(hp)),
        ("HP 1", int(hp)),
        ("slot", int(slot)),
        ("unarmed damage bonus", 0),
        ("unarmed damage bonus 1", 0),
        ("value", 0),
    ]
    ints.sort(key=lambda x: x[0])
    put_i32(body, len(ints))
    for k, v in ints:
        put_pair_i(body, k, v)

    # vec3, vec4
    put_i32(body, 0)
    put_i32(body, 0)

    # strings
    strings = [("description", desc)]
    put_i32(body, len(strings))
    for k, v in strings:
        put_pair_s(body, k, v)

    # FileValue mesh / icon — required. Never ship a mesh-less record.
    files = [
        ("icon", icon),
        ("mesh", mesh),
        ("mesh female", mesh),
    ]
    files.sort(key=lambda x: x[0])
    put_i32(body, len(files))
    for k, v in files:
        put_pair_s(body, k, v)

    # reference categories, instances
    put_i32(body, 0)
    put_i32(body, 0)

    out = bytearray()
    put_i32(out, 4 + len(body))
    out.extend(body)
    return bytes(out)


def write_mod(path: Path) -> bytes:
    items = [write_item(p, i + 1) for i, p in enumerate(PARTS)]
    buf = bytearray()
    put_i32(buf, 16)
    put_i32(buf, 2)
    put_str(buf, "Marf50")
    put_str(buf, DESC)
    put_str(buf, "gamedata.base")
    put_str(buf, "")
    put_i32(buf, len(PARTS))
    put_i32(buf, len(PARTS))
    for it in items:
        buf.extend(it)
    path.write_bytes(buf)
    return bytes(buf)


def parse_mod(data: bytes) -> dict:
    off = 0

    def take_i32() -> int:
        nonlocal off
        (value,) = struct.unpack_from("<i", data, off)
        off += 4
        return value

    def take_str() -> str:
        nonlocal off
        n = take_i32()
        if n < 0 or n > 4000:
            raise SystemExit(f"bad string length {n} at {off}")
        s = data[off : off + n].decode("utf-8")
        off += n
        return s

    header = {
        "type": take_i32(),
        "version": take_i32(),
        "author": take_str(),
        "description": take_str(),
        "dependencies": take_str(),
        "references": take_str(),
        "last_id": take_i32(),
        "item_count": take_i32(),
    }
    if header["type"] != 16:
        raise SystemExit(f"bad type {header['type']}")
    names = []
    for _ in range(header["item_count"]):
        start = off
        length = take_i32()
        itype = take_i32()
        iid = take_i32()
        name = take_str()
        sid = take_str()
        _chg = take_i32()
        if itype != LIMB_REPLACEMENT:
            raise SystemExit(f"item {iid} type {itype} != {LIMB_REPLACEMENT}")

        nbool = take_i32()
        for _b in range(nbool):
            take_str()
            off += 1
        nfloat = take_i32()
        for _f in range(nfloat):
            take_str()
            off += 4
        nint = take_i32()
        for _i in range(nint):
            take_str()
            off += 4
        nvec3 = take_i32()
        off += nvec3 * (4 + 12)  # key length is inside take_str; this is wrong if we skip
        # vec3/vec4 counts are 0 in our writer — refuse anything else
        if nvec3 != 0:
            raise SystemExit(f"{sid} unexpected vec3 count {nvec3}")
        nvec4 = take_i32()
        if nvec4 != 0:
            raise SystemExit(f"{sid} unexpected vec4 count {nvec4}")
        nstr = take_i32()
        for _s in range(nstr):
            take_str()
            take_str()
        nfile = take_i32()
        files = {}
        for _fv in range(nfile):
            key = take_str()
            path = take_str()
            files[key] = path
        if nfile < 2:
            raise SystemExit(f"{sid} has {nfile} FileValues — mesh/icon required")
        if "mesh" not in files or not files["mesh"]:
            raise SystemExit(f"{sid} missing FileValue mesh")
        if ".mesh" not in files["mesh"]:
            raise SystemExit(f"{sid} mesh path has no .mesh: {files['mesh']}")
        if "icon" not in files or not files["icon"]:
            raise SystemExit(f"{sid} missing FileValue icon")
        if "mesh female" not in files or not files["mesh female"]:
            raise SystemExit(f"{sid} missing FileValue mesh female")

        nref = take_i32()
        if nref != 0:
            raise SystemExit(f"{sid} unexpected ref categories {nref}")
        ninst = take_i32()
        if ninst != 0:
            raise SystemExit(f"{sid} unexpected instances {ninst}")

        names.append(f"{iid}:{sid}")
        if off != start + length:
            raise SystemExit(f"{sid} parse ended at {off}, expected {start + length}")
        if off > len(data):
            raise SystemExit("item overran file")
    if off != len(data):
        raise SystemExit(f"trailing {len(data) - off} bytes")
    header["names"] = names
    return header


def write_info(path: Path, mod_filename: str, title: str) -> None:
    path.write_text(
        '<?xml version="1.0" encoding="utf-8"?>\n'
        "<ModData>\n"
        "  <id>0</id>\n"
        f"  <mod>{mod_filename}</mod>\n"
        f"  <title>{title}</title>\n"
        "  <tags>\n"
        "    <string>Gameplay</string>\n"
        "    <string>Races</string>\n"
        "  </tags>\n"
        "  <visibility>0</visibility>\n"
        "  <lastUpdate>2026-08-16</lastUpdate>\n"
        "</ModData>\n",
        encoding="utf-8",
    )


def main() -> None:
    p = argparse.ArgumentParser()
    p.add_argument("--out-dir", type=Path, default=Path("kenshi_mod"))
    p.add_argument("--name", default="LimbVigor")
    p.add_argument("--title", default="Limb Vigor")
    args = p.parse_args()
    args.out_dir.mkdir(parents=True, exist_ok=True)
    mod_path = args.out_dir / f"{args.name}.mod"
    info_path = args.out_dir / f"_{args.name}.info"
    data = write_mod(mod_path)
    header = parse_mod(data)
    write_info(info_path, f"{args.name}.mod", args.title)
    print(f"wrote {mod_path} ({len(data)} bytes)")
    print(f"items {header['item_count']} lastId {header['last_id']}")
    print(" ".join(header["names"]))
    print("FileValue mesh/icon present on all 20 LimbReplacement records")


if __name__ == "__main__":
    main()
