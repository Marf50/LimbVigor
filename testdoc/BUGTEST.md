# Bug-test sheet

Throwaway save. Tick the box. Write the last I-key tooltip line and anything in `RE_Kenshi_log.txt`.

**Where to look:** there is **no** title-screen Limb Vigor box and **no** Blood HUD bars. After the world is in-game, live numbers are on the **C** STATS panel (Hemolymph / Vigor / stage next to Blood) and on the **I-key** LV part. Do not look for a title box.

Every ~15s the log prints `LimbVigor: Boop  Hemolymph 38/100  left leg 4% dormant stump`.

Send the notes back and the plugin gets patched.

- [ ] **Reaches the menu.** `ready v1.9.3 — I-key + C after in-game, no title MyGUI` then `Main menu loaded`. Must NOT log `HUD created at title screen`. Must NOT die after title create / at ~12s with `Log manager destructor`.
- [ ] **Loads the save.** `LimbVigor: In-game` then `player squad seen`. If it does not tick, the log must say **why** (`skip — no player squad` / skeleton / bind failed / …). No 45s / 90s wait. Must NOT touch Character during title / save load (no crash before `In-game`).
- [ ] **Visible after in-game.** Open **C** — Hemolymph / Vigor / stage next to Blood. Open **I** and hover an LV stump/bud/knit part — tooltip shows the same. Do not score Blood HUD bars. Do not look for a title box.
- [ ] **STATS panel.** C is the requested visible path after in-game (`setLineProgress` next to Blood). Not a title MyGUI window.
- [ ] **Heartbeat.** After ~15s in-game, RE_Kenshi_log.txt has a `LimbVigor: <name>  Hemolymph …` line. It must NOT spam `restored limb 0` every frame.
- [ ] **New game.** Same — no crash at the first medical panel.
- [ ] **Shows in the Mods list.** `Kenshi/mods/LimbVigor/LimbVigor.mod` exists. Launcher lists LimbVigor. Enable it after RE_Kenshi.
- [ ] **Hive knits without a kit.** Worker, lost leg, bandaged, fed. I-key slot is `LV Stump/Budding/…`. Stages over ~2.5 days.
- [ ] **Human blocked.** Greenlander, low stats, no splint. Vigor fills. Growth does not start. I-key / C says why on Need.
- [ ] **Splint unlocks a human.** Apply Splint Kit as doctoring. Log: “The splint takes.” Growth starts for ~20h.
- [ ] **Shek 19 vs 20.** 19 blocked, 20 grows. Combat fills Battle-heat faster than standing.
- [ ] **Skeleton never.** Splint, bed, wait. Panel: “Frames do not grow flesh.”
- [ ] **Prosthetic blocks.** Mid-growth, fit a real robot arm, wait, remove it. Progress kept. A bought Economy limb is a real prosthetic — we never slot one as a fake LV part.
- [ ] **Bleed / starve pause.** Open a bleed — pause. Starve — pause + drain. Feed + bandage — resume.
- [ ] **Bed is faster.** Twin hivers, one in a bed. About 2×.
- [ ] **Legs first.** Missing a leg and an arm. Leg finishes first.
- [ ] **I-key growth part.** Select the stump character, open inventory (I). The missing limb slot should show `LV Stump …` / `LV Budding …` / `LV Forming …` / `LV Knitting …` / `LV Grown …`, not empty and not a bought Economy limb. If LimbVigor.mod missed or a record has no mesh/icon, the log says `skip (not using Economy)` and we leave the socket alone.
- [ ] **Not a real prosthetic.** Fitting a growth part must NOT stop Hemolymph. Buying a real robot limb still blocks.
- [ ] **Grown is the limb.** At 100% the I-key slot should say `LV Grown …`, not −15 and not original flesh. We do not call `setLimb(ORIGINAL)`. Send the log either way. Boop's already-100% left leg should become Grown on load.
- [ ] **Save / load.** Grow to ~40%, save, quit, load. Numbers persist (LimbVigor.progress).
- [ ] **Squad.** Player + hired hive + hired shek, each selected I-key shows their own numbers.
- [ ] **No title-screen crash.** No TitleScreen widget create. No MyGUI mouse delegates. No eventFrameStart. Dark UI must reach the menu.

## Notes

Race / limb / bed / fed / combat / I-key text / log:
