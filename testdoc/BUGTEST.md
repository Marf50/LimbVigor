# Bug-test sheet

Throwaway save. Tick the box. Write what the left Blood HUD bar says and anything in `RE_Kenshi_log.txt`.

**Where to look:** after the world is in-game, a **new left-stack bar under Blood** shows Hemolymph / Vigor / Battle-heat (fill + number). Backup is the **I-key** LV part tooltip. There is **no** title-screen Limb Vigor box. **C is skills** — not the scorecard. Do not look for a Blood line on C. Do not score a ProgressBar skin.

Every ~15s the log prints `LimbVigor: Boop  Hemolymph 38/100  left leg 4% dormant stump`.

Send the notes back and the plugin gets patched.

- [ ] **Reaches the menu.** `ready v1.9.4 — Blood HUD bar after in-game, no title MyGUI` then `Main menu loaded`. Must NOT log `HUD created at title screen`. Must NOT die after title create / at ~12s with `Log manager destructor`.
- [ ] **Loads the save.** `LimbVigor: In-game` then `player squad seen`. Then `LimbVigor: Blood HUD created after in-game` (or a clear create-fail / caption-only line). If it does not tick, the log must say **why** (`skip — no player squad` / skeleton / bind failed / …). Must NOT touch Character during title / save load.
- [ ] **Visible after in-game.** Left HUD, under Blood: Hemolymph / Vigor / Battle-heat bar with fill + number. Open **I** and hover an LV stump/bud/knit part — tooltip is the backup. Do not open C for this. Do not look for a title box. No ProgressBar skin.
- [ ] **Heartbeat.** After ~15s in-game, RE_Kenshi_log.txt has a `LimbVigor: <name>  Hemolymph …` line. It must NOT spam `restored limb 0` every frame.
- [ ] **New game.** Same — no crash at the first medical panel. Bar appears after In-game, not at the title.
- [ ] **Shows in the Mods list.** `Kenshi/mods/LimbVigor/LimbVigor.mod` exists. Launcher lists LimbVigor. Enable it after RE_Kenshi.
- [ ] **Hive knits without a kit.** Worker, lost leg, bandaged, fed. I-key slot is `LV Stump/Budding/…`. Stages over ~2.5 days.
- [ ] **Human blocked.** Greenlander, low stats, no splint. Vigor fills. Growth does not start. Left bar / I-key says why on Need.
- [ ] **Splint unlocks a human.** Apply Splint Kit as doctoring. Log: “The splint takes.” Growth starts for ~20h.
- [ ] **Shek 19 vs 20.** 19 blocked, 20 grows. Combat fills Battle-heat faster than standing.
- [ ] **Skeleton never.** Splint, bed, wait. Bar: “Frames do not grow flesh.”
- [ ] **Prosthetic blocks.** Mid-growth, fit a real robot arm, wait, remove it. Progress kept. A bought Economy limb is a real prosthetic — we never slot one as a fake LV part.
- [ ] **Bleed / starve pause.** Open a bleed — pause. Starve — pause + drain. Feed + bandage — resume.
- [ ] **Bed is faster.** Twin hivers, one in a bed. About 2×.
- [ ] **Legs first.** Missing a leg and an arm. Leg finishes first.
- [ ] **I-key growth part.** Select the stump character, open inventory (I). The missing limb slot should show `LV Stump …` / `LV Budding …` / `LV Forming …` / `LV Knitting …` / `LV Grown …`, not empty and not a bought Economy limb. If LimbVigor.mod missed or a record has no mesh/icon, the log says `skip (not using Economy)` and we leave the socket alone.
- [ ] **Not a real prosthetic.** Fitting a growth part must NOT stop Hemolymph. Buying a real robot limb still blocks.
- [ ] **Grown is the limb.** At 100% the I-key slot should say `LV Grown …`, not −15 and not original flesh. We do not call `setLimb(ORIGINAL)`. Send the log either way. Boop's already-100% left leg should become Grown on load.
- [ ] **Save / load.** Grow to ~40%, save, quit, load. Numbers persist (LimbVigor.progress). Bar comes back after In-game, not at title.
- [ ] **Squad.** Player + hired hive + hired shek, each selected I-key shows their own numbers. Left bar follows the selected body.
- [ ] **No title-screen crash.** No TitleScreen widget create. No MyGUI mouse delegates. No eventFrameStart. Dark UI must reach the menu.

## Notes

Race / limb / bed / fed / combat / left bar text / I-key text / log:
