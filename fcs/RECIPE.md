# FCS companion — Limb Vigor

`LimbVigor.mod` already contains 20 `LimbReplacement` records
(stump / budding / forming / knitting / grown × four sockets), each
with FileValue mesh/icon. Do not duplicate those by hand —
`tools/write_parts_mod.py` is the source. Do not ship a mesh-less
record; C++ will refuse it and will not fall back to Economy.

Optional extra: named drugs on shelves.

Create a new mod named `Limb Vigor Drugs` if you want shop items. Load `gamedata.base` + your file.

Duplicate **Splint Kit** three times. Keep the same item function (doctoring / first aid family). The plugin name-matches these words: `splint`, `regrowth`, `stimulant`, `growth kit`, `ichor`, `bone-knit`, `marrow`.

| Name | Value | Weight | Description |
| --- | --- | --- | --- |
| Bone-knit Stimulant | 900 | 0.2 | Applied as doctoring to a stump. Humans who have not earned the body can grow from this for about a day. |
| Marrow Salve | 600 | 0.3 | Shek camp grease. Younger warriors use it until toughness does the job alone. |
| Chitin Ichor | 400 | 0.2 | Hive pot. Not required. A drone already knits. |

Book, type Book, name `Notes on the Stump`, value 250:

> Hive blood knits chitin if the drone is fed. Shek marrow remembers after enough hits, or after a splint. A human needs a Splint Kit, a stimulant, or a body that has already been broken often (toughness 40, medic 25). Skeletons do not grow flesh. A robot limb fills the socket and stops the stump. The bar sits next to Blood.

Vendors: World’s End medical, Squin / Admag, Hive village shops. Keep counts low.

Do not change race heal rates. That heals HP. It does not restore a destroyed limb.
