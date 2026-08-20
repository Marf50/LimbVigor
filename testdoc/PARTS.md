# Growth parts

A STUMP socket is not attached until `EquipWriteSeh` succeeds this session.
Persist 100% + still STUMP is not done. Sync still attempts the first STUMP
write (`createItem` + `SlotPart`, `setLimb REPLACED`).

First write is always the STUMP nub. After that, stages are part-type swaps:
BUD → FORM → KNIT, then GROWN. Never Equip GROWN first. Never
`setLimb(ORIGINAL)`.

Do not grow a missing stump with flesh/max ticks (`LvGrowStumpNub` is not
the player-visible product).

## Log

Success: `left leg STUMP → budding nub attached hp=…` (or a later stage
swap with `nub attached`).

No write: `skip why=` one of `have` / `prosthetic` / `sehSkip` / `no-gd` /
`failUntil` / `create-failed`. SEH also logs `nub attach SEH skip`.

`nubWrote` is session-only. The persist file may already say 100% / Grown;
that does not skip the first STUMP write.

## LimbVigor.mod FileValues

Every LV record (`lv-stump-l-leg` and the other 19) must have FileValue
mesh + icon. Paths are the vanilla Economy visuals already in
`src/lv_parts.cpp` (`LV_MESH_*` / `LV_ICON_*`). Still our item — never
`createItem(Economy)`. A leftover `nfile=0` record fail-closes
`RecordCanEquip` (`skip why=no-gd`). `tools/check_parts_mod.py` walks
the committed file the same way C++ does.
