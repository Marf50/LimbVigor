# Bug-test sheet

Throwaway save. Tick the box. Write the last I-key tooltip line and anything in `RE_Kenshi_log.txt`.

**Where to look:** select a **character**. The selection info list (Blood) should show Hemolymph / Vigor / Battle-heat via `setLineProgress` (`72 / 100`) on Blood’s category. Must NOT overwrite Blood, Head, limbs, or Hunger. Spoken lines over the character. I-key on the LV part is the backup. **C is skills**. Door or anything non-character: Hemolymph line is hidden. Walk is that selection panel: `panel=` vs `medicalPanel=`, `lineExists(Blood)`, `getNumLines`, every `key=`. If none exists: `_NV_say` only and log `LimbVigor: none exists`. No layout file. Must NOT log `HUD created` or `Blood HUD created after in-game`. No sibling create.

Every ~15s the log prints `LimbVigor: Boop  Hemolymph 38/100  left leg 4% dormant stump`.

Send the notes back and the plugin gets patched.

- [ ] **Reaches the menu.** `ready v1.9.7 — setLineProgress on medicalPanel + _NV_say, no createWidget` then `Main menu loaded`. Must NOT log `HUD created at title screen` or `Blood HUD created after in-game`. Must NOT die after title create / at ~12s with `Log manager destructor`.
- [ ] **Loads the save.** `LimbVigor: In-game` (or `In-game — ignoring stuck gameResetting`) then `player squad seen`. A −15 empty stump must get an LV part (`slotted LV Stump/Grown …`) or a logged skip saying why. If it does not tick, the log must say **why**. Must NOT touch Character during title / save load. Must NOT crash after In-game.
- [ ] **Visible after in-game.** Select a character. Selection list (Blood) gets Hemolymph / Vigor / Battle-heat (`72 / 100`) via `setLineProgress` on Blood’s category. Log: `panel=` vs `medicalPanel=` / `lineExists(Blood)` / `getNumLines` / every `key=`. Blood / Head / limbs / Hunger unchanged. Spoken line. Open **I** as backup. C stays skills. Door or anything non-character: Hemolymph hidden. If none exists: `_NV_say` only and `LimbVigor: none exists`. No layout file.
- [ ] **Heartbeat.** After ~15s in-game, RE_Kenshi_log.txt has a `LimbVigor: <name>  Hemolymph …` line. It must NOT spam `restored limb 0` every frame.
- [ ] **New game.** Same — no crash at the first medical panel. No MyGUI create.
- [ ] **Shows in the Mods list.** `Kenshi/mods/LimbVigor/LimbVigor.mod` exists. Launcher lists LimbVigor. Enable it after RE_Kenshi.
- [ ] **Hive knits without a kit.** Worker, lost leg, bandaged, fed. I-key slot is `LV Stump/Budding/…`. Stages over ~2.5 days.
- [ ] **Human blocked.** Greenlander, low stats, no splint. Vigor fills. Growth does not start. I-key says why on Need.
- [ ] **Splint unlocks a human.** Apply Splint Kit as doctoring. Log: “The splint takes.” Growth starts for ~20h.
- [ ] **Shek 19 vs 20.** 19 blocked, 20 grows. Combat fills Battle-heat faster than standing.
- [ ] **Skeleton never.** Splint, bed, wait. I-key / log: “Frames do not grow flesh.”
- [ ] **Prosthetic blocks.** Mid-growth, fit a real robot arm, wait, remove it. Progress kept. A bought Economy limb is a real prosthetic — we never slot one as a fake LV part.
- [ ] **Bleed / starve pause.** Open a bleed — pause. Starve — pause + drain. Feed + bandage — resume.
- [ ] **Bed is faster.** Twin hivers, one in a bed. About 2×.
- [ ] **Legs first.** Missing a leg and an arm. Leg finishes first.
- [ ] **I-key growth part.** Select the stump character, open inventory (I). The missing limb slot should show `LV Stump …` / `LV Budding …` / `LV Forming …` / `LV Knitting …` / `LV Grown …`, not empty and not a bought Economy limb. If LimbVigor.mod missed or a record has no mesh/icon, the log says `skip (not using Economy)` and we leave the socket alone.
- [ ] **Not a real prosthetic.** Fitting a growth part must NOT stop Hemolymph. Buying a real robot limb still blocks.
- [ ] **Grown is the limb.** At 100% the I-key slot should say `LV Grown …`, not −15 and not original flesh. We do not call `setLimb(ORIGINAL)`. Send the log either way. Boop's already-100% left leg should become Grown on load.
- [ ] **Save / load.** Grow to ~40%, save, quit, load. Numbers persist (LimbVigor.progress). No MyGUI create after In-game.
- [ ] **Squad.** Player + hired hive + hired shek, each selected I-key shows their own numbers.
- [ ] **No MyGUI crash.** No TitleScreen widget create. No in-game widget create. No MyGUI mouse delegates. No eventFrameStart. Dark UI must reach the menu and survive save load.

## Notes

Race / limb / bed / fed / combat / I-key text / log:
