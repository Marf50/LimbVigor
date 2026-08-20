#!/usr/bin/env python3
"""Walk committed kenshi_mod/LimbVigor.mod the same way C++ does.

ModRecordHasMeshIcon: type=16, records type=111, length-prefixed.
FileValue key containing "mesh" + value containing ".mesh",
and key containing "icon" + non-empty value.
nfile=0 on any LV record is a fail.
"""
from __future__ import annotations

import struct
import sys
from pathlib import Path

NEED = [
    "lv-stump-r-leg", "lv-bud-r-leg", "lv-form-r-leg", "lv-knit-r-leg", "lv-grown-r-leg",
    "lv-stump-l-leg", "lv-bud-l-leg", "lv-form-l-leg", "lv-knit-l-leg", "lv-grown-l-leg",
    "lv-stump-r-arm", "lv-bud-r-arm", "lv-form-r-arm", "lv-knit-r-arm", "lv-grown-r-arm",
    "lv-stump-l-arm", "lv-bud-l-arm", "lv-form-l-arm", "lv-knit-l-arm", "lv-grown-l-arm",
]

MESH = {
    "r-leg": r".\data\items\robotics\economy leg R.mesh",
    "l-leg": r".\data\items\robotics\economy leg L.mesh",
    "r-arm": r".\data\items\robotics\economy arm R.mesh",
    "l-arm": r".\data\items\robotics\economy arm L.mesh",
}
ICON = {
    "r-leg": r".\data\items\robotics\economy leg R.png",
    "l-leg": r".\data\items\robotics\economy leg L.png",
    "r-arm": r".\data\items\robotics\economy arm R.png",
    "l-arm": r".\data\items\robotics\economy arm L.png",
}


def take_i32(data: bytes, off: list) -> int:
    (v,) = struct.unpack_from("<i", data, off[0])
    off[0] += 4
    return v


def take_str(data: bytes, off: list) -> str:
    n = take_i32(data, off)
    if n < 0 or n > 4000 or off[0] + n > len(data):
        raise SystemExit(f"bad string length {n} at {off[0]}")
    s = data[off[0] : off[0] + n].decode("utf-8")
    off[0] += n
    return s


def limb_key(sid: str) -> str:
    for key in ("r-leg", "l-leg", "r-arm", "l-arm"):
        if sid.endswith(key):
            return key
    raise SystemExit(f"unknown limb in {sid}")


def parse_mod(data: bytes) -> list:
    off = [0]
    typ = take_i32(data, off)
    ver = take_i32(data, off)
    take_str(data, off)
    take_str(data, off)
    take_str(data, off)
    take_str(data, off)
    take_i32(data, off)
    count = take_i32(data, off)
    if typ != 16:
        raise SystemExit(f"bad type {typ}")
    if ver < 1:
        raise SystemExit(f"bad version {ver}")
    if count != 20:
        raise SystemExit(f"want 20 records, got {count}")

    rows = []
    for _ in range(count):
        start = off[0]
        length = take_i32(data, off)
        itype = take_i32(data, off)
        take_i32(data, off)
        take_str(data, off)
        sid = take_str(data, off)
        take_i32(data, off)
        if itype != 111:
            raise SystemExit(f"{sid} type {itype} != 111")

        nbool = take_i32(data, off)
        if nbool < 0 or nbool > 32:
            raise SystemExit(f"{sid} bad nbool {nbool}")
        for _b in range(nbool):
            take_str(data, off)
            off[0] += 1

        nfloat = take_i32(data, off)
        if nfloat < 0 or nfloat > 64:
            raise SystemExit(f"{sid} bad nfloat {nfloat}")
        for _f in range(nfloat):
            take_str(data, off)
            off[0] += 4

        nint = take_i32(data, off)
        if nint < 0 or nint > 64:
            raise SystemExit(f"{sid} bad nint {nint}")
        for _i in range(nint):
            take_str(data, off)
            off[0] += 4

        nvec3 = take_i32(data, off)
        nvec4 = take_i32(data, off)
        if nvec3 != 0 or nvec4 != 0:
            raise SystemExit(f"{sid} unexpected vec3/vec4 {nvec3}/{nvec4}")

        nstr = take_i32(data, off)
        if nstr < 0 or nstr > 16:
            raise SystemExit(f"{sid} bad nstr {nstr}")
        for _s in range(nstr):
            take_str(data, off)
            take_str(data, off)

        nfile = take_i32(data, off)
        if nfile < 0 or nfile > 16:
            raise SystemExit(f"{sid} bad nfile {nfile}")
        mesh = 0
        icon = 0
        files = []
        for _fv in range(nfile):
            key = take_str(data, off)
            val = take_str(data, off)
            files.append((key, val))
            if "mesh" in key.lower() and val and ".mesh" in val.lower():
                mesh = 1
            if "icon" in key.lower() and val:
                icon = 1

        end = start + length
        if end <= start or end > len(data):
            raise SystemExit(f"{sid} bad length {length} start={start}")
        # C++ jumps here. Tail is nref/ninst (usually 0,0).
        off[0] = end
        rows.append({
            "sid": sid,
            "nfile": nfile,
            "mesh": mesh,
            "icon": icon,
            "files": files,
            "length": length,
        })

    if off[0] != len(data):
        raise SystemExit(f"trailing {len(data) - off[0]} bytes — length walk broke")
    return rows


def check(path: Path) -> None:
    rows = parse_mod(path.read_bytes())
    got = [r["sid"] for r in rows]
    missing = [s for s in NEED if s not in got]
    if missing:
        raise SystemExit(f"missing stringIds: {missing}")
    fail = []
    for r in rows:
        if r["nfile"] < 2 or not r["mesh"] or not r["icon"]:
            fail.append(f"{r['sid']} nfile={r['nfile']} mesh={r['mesh']} icon={r['icon']}")
    if fail:
        raise SystemExit("mesh-less LV records:\n  " + "\n  ".join(fail))
    print(f"ok  LimbVigor.mod {len(rows)} records nfile>=2 mesh=1 icon=1")
    for r in rows:
        print(f"  {r['sid']} nfile={r['nfile']} mesh={r['mesh']} icon={r['icon']}")


def put_str(text: str) -> bytes:
    raw = text.encode("utf-8")
    return struct.pack("<i", len(raw)) + raw


def inject(path: Path) -> None:
    data = bytearray(path.read_bytes())
    off = [0]
    take_i32(data, off)
    take_i32(data, off)
    take_str(data, off)
    take_str(data, off)
    take_str(data, off)
    take_str(data, off)
    take_i32(data, off)
    count = take_i32(data, off)
    out = bytearray(data[: off[0]])
    for _ in range(count):
        start = off[0]
        length = take_i32(data, off)
        rec_end = start + length
        itype = take_i32(data, off)
        take_i32(data, off)
        take_str(data, off)
        sid = take_str(data, off)
        take_i32(data, off)
        if itype != 111:
            raise SystemExit(f"{sid} type {itype} != 111")
        nbool = take_i32(data, off)
        for _b in range(nbool):
            take_str(data, off)
            off[0] += 1
        nfloat = take_i32(data, off)
        for _f in range(nfloat):
            take_str(data, off)
            off[0] += 4
        nint = take_i32(data, off)
        for _i in range(nint):
            take_str(data, off)
            off[0] += 4
        nvec3 = take_i32(data, off)
        nvec4 = take_i32(data, off)
        if nvec3 or nvec4:
            raise SystemExit(f"{sid} unexpected vec")
        nstr = take_i32(data, off)
        for _s in range(nstr):
            take_str(data, off)
            take_str(data, off)
        nfile_pos = off[0]
        nfile = take_i32(data, off)
        for _fv in range(nfile):
            take_str(data, off)
            take_str(data, off)
        mid = off[0]
        tail = bytes(data[mid:rec_end])
        prefix = bytes(data[start + 4 : nfile_pos])
        limb = limb_key(sid)
        files = put_str("icon") + put_str(ICON[limb]) + put_str("mesh") + put_str(MESH[limb])
        body = prefix + struct.pack("<i", 2) + files + tail
        out.extend(struct.pack("<i", 4 + len(body)))
        out.extend(body)
        off[0] = rec_end
    path.write_bytes(out)
    print(f"wrote {path} ({len(out)} bytes)")


def main() -> None:
    root = Path(__file__).resolve().parents[1]
    path = root / "kenshi_mod" / "LimbVigor.mod"
    if len(sys.argv) > 1 and sys.argv[1] == "--inject":
        inject(path)
    check(path)


if __name__ == "__main__":
    main()
