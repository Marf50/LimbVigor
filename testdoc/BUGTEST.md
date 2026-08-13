# Bug-test sheet

Throwaway save. Tick the box. Write the last STATS-panel line and anything in `RE_Kenshi_log.txt`.

**Where to look:** click the character, open STATS (key C). Under Blood: Hemolymph / Battle-heat / Vigor, plus a Regrowth line if they have a stump. Every ~15s the log prints `LimbVigor: Boop  Hemolymph 38/100  left leg 4% dormant stump`.

Send the notes back and the plugin gets patched.

- [ ] **Loads a save.** World's End / any town. No crash. RE_Kenshi_log shows `LimbVigor: first player-squad tick` after a few seconds.
- [ ] **STATS panel.** Select a squad member with a stump. Blood list shows the resource bar and a Regrowth line. Not the portrait strip.
- [ ] **Heartbeat.** After ~15s, RE_Kenshi_log.txt has a `LimbVigor: <name>  Hemolymph …` line. It must NOT spam `restored limb 0` every frame.
- [ ] **New game.** Same — no crash at the first medical panel.
- [ ] **Shows in the Mods list.** `Kenshi/mods/LimbVigor/LimbVigor.mod` exists. Launcher lists LimbVigor. Enable it after RE_Kenshi.
- [ ] **Hive knits without a kit.** Worker, lost leg, bandaged, fed. Hemolymph under Blood. Stages over ~2.5 days.
- [ ] **Human blocked.** Greenlander, low stats, no splint. Vigor fills. Growth does not start. Panel says why.
- [ ] **Splint unlocks a human.** Apply Splint Kit as doctoring. Log: “The splint takes.” Growth starts for ~20h.
- [ ] **Shek 19 vs 20.** 19 blocked, 20 grows. Combat fills Battle-heat faster than standing.
- [ ] **Skeleton never.** Splint, bed, wait. Panel: “Frames do not grow flesh.”
- [ ] **Prosthetic blocks.** Mid-growth, fit a robot arm, wait, remove it. Progress kept.
- [ ] **Bleed / starve pause.** Open a bleed — pause. Starve — pause + drain. Feed + bandage — resume.
- [ ] **Bed is faster.** Twin hivers, one in a bed. About 2×.
- [ ] **Legs first.** Missing a leg and an arm. Leg finishes first.
- [ ] **Weak return.** New limb is flesh at ~22% HP, needs first aid, not a prosthetic.
- [ ] **Save / load.** Grow to ~40%, save, quit, load. Numbers persist (LimbVigor.progress).
- [ ] **Squad.** Player + hired hive + hired shek, each selected shows their own bar.

## Notes

Race / limb / bed / fed / combat / panel text / log:
