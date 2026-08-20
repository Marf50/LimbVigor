# Bug-test sheet

Throwaway save. Tick the box. Write the last I-key tooltip line and anything in `RE_Kenshi_log.txt`.

**Where to look:** select a **character** after In-game. An extra bar **tight under Hunger** (same plate, looks like Blood — internal name `HemolymphBar`, not LifeBar10) at the **current** v1.33 Root-after-MedicalPanel coords, Depth=1 — not `LifeBar10Slot`, not last-child-of-Front. **Hemolymph** / Vigor / Battle-heat centered on the left, **number** on the right, green fill when full. Blood stays Blood. LifeBar1–9 unstretched as in this Dark UI override (do **not** rebase onto vanilla 1–9 / MedicalPanel y `0.712963`). Floor / Speed stay uncovered. Door clears our bar only. Designer look before **and** after ESC. Bar gone after ESC = fail. See testdoc/HUD.md.

Dylan's live pack is the unbound-slot class Kenshi survives: vanilla MainPanel, LifeBar1–9 only, LifeBar10 slot null. Our extra named LifeBar10 was the ownership bug. HemolymphBar is ours. Hidden LifeBar11 is not required.

Every ~15s the log prints `LimbVigor: Boop  Hemolymph 38/100  left leg 4% dormant`.

Send the notes back and the plugin gets patched.

- [ ] **Reaches the menu.** Log `HUD HemolymphBar — LifeBar10 slot empty, re-find each paint` then `Main menu loaded`. Must NOT log `ready v1.39`. Must NOT log `HUD created at title screen`. Must NOT hook MainBarGUI ctor `0x72C1E0`. Must NOT walk the Gui tree. Must NOT `setSize` except HemolymphBarGreen / a 0-size HemolymphBarDatapanel. Must NOT `setVisible` on Root / MedicalPanel / Back / Front / LifeBar1–9. Must NOT call `_getWidget`. Must NOT contain `HemolymphStrip`, `LifeBar10Slot`, or a widget named `LifeBar10` / `LifeBar11`. Must NOT skip paint because PausedPanel vis=1. Must NOT hook ESC or the injected settings button.
- [ ] **Loads the save.** `LimbVigor: In-game` (or `In-game — ignoring stuck gameResetting`) then `player squad seen`. A −15 empty stump must get an LV part or log `nub attach SEH skip` / why. Must NOT touch Character during title / save load. Must NOT crash after In-game.
- [ ] **Person-select shows Hemolymph under Hunger.** Select a **person**. LifeBar1–9 stay in their Dark UI slots. Extra bar tight under Hunger (same plate, looks like Blood). **Hemolymph** centered left, **99** right, green fill. After ESC the bar must still be there — gone = fail. Kenshi's LifeBar10* must stay null (no widget named LifeBar10). ESC / settings close must leave Kenshi alive. Must NOT die with zero LimbVigor lines after `Injected settings button`. 75-HP arms stay 75. Send `RE_Kenshi_log.txt`.
- [ ] **HUD SEH does not kill growth.** If a HUD write excepts, growth / heartbeat keep ticking. Must NOT latch `medical row stopped` forever. Must NOT log 500+ empty panel walks. Must NOT die after one `medical tick SEH`.
- [ ] **Selected body, not the player pawn.** Select a **hired Hive** and a **hired Shek**. HemolymphBar is that body's Hemolymph / Battle-heat on screen. Blood stays Blood. I-key snap matches the selected body. Door / building: clear HemolymphBar only. Must NOT only paint the player character. Must NOT shift LifeBar1–9.
- [ ] **First-stump bark.** A new stump (not an already-missing limb on load) speaks once: Hive `The stump itches. It will grow.` / Shek `The stump wants a fight, or a splint.` / Human `The stump will not grow on its own.` Hired squad bodies bark too. Must NOT spam every tick or on save load.
- [ ] **Heartbeat.** After ~15s in-game, RE_Kenshi_log.txt has a `LimbVigor: <name>  Hemolymph …` line. It must NOT spam `restored limb 0` every frame.
- [ ] **New game.** Same — no crash at the first medical panel. No MyGUI create.
- [ ] **Shows in the Mods list.** `Kenshi/mods/LimbVigor/LimbVigor.mod` exists. Launcher lists LimbVigor. Enable it after RE_Kenshi.
- [ ] **Hive knits without a kit.** Worker, lost leg, bandaged, fed. I-key slot is `LV Stump/Budding/…`. Stages over ~2.5 days.
- [ ] **Human blocked.** Greenlander, low stats, no splint. Vigor fills. Growth does not start. Line 2 is the Splint Kit reason.
- [ ] **Splint unlocks a human.** Apply Splint Kit as doctoring. Log: “The splint takes.” Growth starts for ~20h.
- [ ] **Shek 19 vs 20.** 19 blocked, 20 grows. Combat fills Battle-heat faster than standing.
- [ ] **Skeleton never.** Splint, bed, wait. I-key / log: “Frames do not grow flesh.”
- [ ] **Prosthetic blocks.** Mid-growth, fit a real robot arm, wait, remove it. Progress kept. A bought Economy limb is a real prosthetic — we never slot one as a fake LV part.
- [ ] **Bleed / starve pause.** Open a bleed — pause. Starve — pause + drain. Feed + bandage — resume.
- [ ] **Bed is faster.** Twin hivers, one in a bed. About 2×.
- [ ] **Legs first.** Missing a leg and an arm. Leg finishes first.
- [ ] **I-key growth part.** Select the stump character, open inventory (I). The missing limb slot should show `LV Stump …` / `LV Budding …` / `LV Forming …` / `LV Knitting …` / `LV Grown …`, not empty and not a bought Economy limb. If LimbVigor.mod missed or a record has no mesh/icon, the log says `skip (not using Economy)` and we leave the socket alone.
- [ ] **Not a real prosthetic.** Fitting a growth part must NOT stop Hemolymph. Buying a real robot limb still blocks.
- [ ] **Grown is the limb.** Stages increment: STUMP nub first, then Budding → Forming → Knitting → Grown. At 100% the I-key slot should say `LV Grown …`, not −15 and not original flesh. We do not call `setLimb(ORIGINAL)`. Never Equip GROWN as the first write. Send the log either way.
- [ ] **Save / load.** Grow to ~40%, save, quit, load. Numbers persist (LimbVigor.progress). No MyGUI create after In-game.
- [ ] **Squad.** Player + hired hive + hired shek, each selected I-key shows their own numbers.
- [ ] **No MyGUI crash.** No TitleScreen widget create. No in-game widget create. No MyGUI mouse delegates. No eventFrameStart. Dark UI must reach the menu and survive save load.

## Notes

Race / limb / bed / fed / combat / I-key text / log:
