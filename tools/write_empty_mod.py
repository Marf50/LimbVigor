#!/usr/bin/env python3
"""Write a valid empty Kenshi FCS .mod (type 16) plus _Name.info.

Format matches OpenConstructionSet DataFileType.Mod:
  int32 type=16
  int32 version
  string author, description, dependencies, references
  int32 lastId
  int32 itemCount=0

Strings are little-endian length + UTF-8 bytes (no NUL).
"""
from __future__ import annotations

import argparse
import struct
from pathlib import Path

DESC = (
    "Organic races can grow a lost limb back. "
    "Hive: innate hemolymph. "
    "Shek: toughness 20 or a used Splint Kit. "
    "Human: toughness 40 and Field Medic 25, or a used Splint Kit. "
    "Skeletons never. "
    "The bar sits under Blood. Needs RE_Kenshi."
)


def put_i32(buf: bytearray, value: int) -> None:
    buf.extend(struct.pack("<i", value))


def put_str(buf: bytearray, text: str) -> None:
    data = text.encode("utf-8")
    put_i32(buf, len(data))
    buf.extend(data)


def write_mod(
    path: Path,
    *,
    author: str = "Marf50",
    description: str = DESC,
    dependencies: str = "gamedata.base",
    version: int = 1,
    last_id: int = 1,
) -> bytes:
    buf = bytearray()
    put_i32(buf, 16)  # DataFileType.Mod
    put_i32(buf, version)
    put_str(buf, author)
    put_str(buf, description)
    put_str(buf, dependencies)
    put_str(buf, "")  # references
    put_i32(buf, last_id)
    put_i32(buf, 0)  # no records — plugin does the work
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
        "bytes_left": len(data) - off,
    }
    if header["type"] != 16:
        raise SystemExit(f"bad type {header['type']}")
    if header["item_count"] != 0 or header["bytes_left"] != 0:
        raise SystemExit(f"not an empty mod: {header}")
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
        "  <lastUpdate>2026-08-13</lastUpdate>\n"
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
    print(f"wrote {mod_path} ({len(data)} bytes) {header}")
    print(f"wrote {info_path}")


if __name__ == "__main__":
    main()
