# Bug-test sheet

Throwaway save. Tick the box. Write the last STATS-panel line and anything in `RE_Kenshi_log.txt`.

**Where to look:** select a squad member. Left HUD under Blood: Hemolymph / Battle-heat / Vigor bar with the number on it. Hover the bar — tooltip is the same text as the line under it (time + race rule). Stump adds a second bar. Open **I** and hover the LV part — tooltip has live resource / % / time. C is a backup list.

Every ~15s the log prints `LimbVigor: Boop  Hemolymph 38/100  left leg 4% dormant stump`.

Send the notes back and the plugin gets patched.

- [ ] **Reaches the menu and the save.** `ready v1.8.9` then `HUD created at title screen (click/hover to refresh)` then `Main menu loaded` then `In-game`. Then `player squad seen` then ~45s later `ticks on` then ~90s later `parts on`. Hover the Limb Vigor box — it should stop saying `hover after load` and show Hemolymph / Vigor.
- [ ] **Left HUD widgets.** Select a squad member. Under Blood: resource bar with the number on it. You can read it without hovering. Hover it — tooltip matches the line under the bar.
- [ ] **STATS panel.** C still lists Hemolymph / Regrowth / Time. Backup only.
- [ ] **I-key tooltip.** Open inventory. Socket says `LV Stump/Budding/Forming/Knitting/Grown …`. Hover it — description has Hemolymph / stage / time, not a silent Economy limb blurb.
- [ ] **Heartbeat.** After ~15s, RE_Kenshi_log.txt has a `LimbVigor: <name>  Hemolymph …` line. It must NOT spam `restored limb 0` every frame.
- [ ] **New game.** Same — no crash at the first medical panel.
- [ ] **Shows in the Mods list.** `Kenshi/mods/LimbVigor/LimbVigor.mod` exists. Launcher lists LimbVigor. Enable it after RE_Kenshi.
- [ ] **Hive knits without a kit.** Worker, lost leg, bandaged, fed. Hemolymph under Blood. Stages over ~2.5 days.
- [ ] **Human blocked.** Greenlander, low stats, no splint. Vigor fills. Growth does not start. Panel says why on Need.
- [ ] **Splint unlocks a human.** Apply Splint Kit as doctoring. Log: “The splint takes.” Growth starts for ~20h.
- [ ] **Shek 19 vs 20.** 19 blocked, 20 grows. Combat fills Battle-heat faster than standing.
- [ ] **Skeleton never.** Splint, bed, wait. Panel: “Frames do not grow flesh.”
- [ ] **Prosthetic blocks.** Mid-growth, fit a robot arm, wait, remove it. Progress kept.
- [ ] **Bleed / starve pause.** Open a bleed — pause. Starve — pause + drain. Feed + bandage — resume.
- [ ] **Bed is faster.** Twin hivers, one in a bed. About 2×.
- [ ] **Legs first.** Missing a leg and an arm. Leg finishes first.
- [ ] **I-key growth part.** Select the stump character, open inventory (I). The missing limb slot should show `LV Stump …` / `LV Budding …` / `LV Forming …` / `LV Knitting …` / `LV Grown …`, not empty and not a bought Economy limb (unless the .mod failed to load — then the log says `economy`). Hover the slot: stats should be worse than a real leg and improve as the bar climbs.
- [ ] **Not a real prosthetic.** Fitting a growth part must NOT stop Hemolymph. The STATS line still ticks. Buying a real robot limb still blocks.
- [ ] **Grown is the limb.** At 100% the I-key slot should say `LV Grown …`, not −15 and not original flesh. Send the log either way. Boop's already-100% left leg should become Grown on load.
- [ ] **Save / load.** Grow to ~40%, save, quit, load. Numbers persist (LimbVigor.progress).
- [ ] **Squad.** Player + hired hive + hired shek, each selected shows their own STATS lines.
- [ ] **Dark UI.** Overlay still appears under Blood (Middle layer). Game does not crash on load.

## Notes

Race / limb / bed / fed / combat / panel text / log:
