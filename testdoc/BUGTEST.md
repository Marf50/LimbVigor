# Bug-test sheet

Throwaway save. Tick the box. Write the last I-key tooltip line and anything in `RE_Kenshi_log.txt`.

**Where to look:** select a **character** after In-game. A **new row under Hunger** (LifeBar10) must show **Hemolymph** (or Vigor / Battle-heat). Blood stays Blood and stays in its slot. Head/Stomach/Chest/arms/legs/Hunger stay in their slots (same pixel size). Name plate / Squad / Floor / speed chrome must not drift. Log must dump every MyGUI `setCaption` export, bind `Widget::setCaption` (`setCapW=1`, not TextBox-only), `setCapISub` if present, `setVisible` on LifeBar10 / Datapanel / Value / Green after orig, `vis=1` after show, and `getCaption` read-back must be Hemolymph (not Blood / empty). `painted=1` only if vis=1 and Hemolymph/Vigor/Battle-heat. Must NOT call `_getWidget`. Door hides/clears LifeBar10* only. Layout sha256 stays `d7a31296…`. See testdoc/HUD.md.

Every ~15s the log prints `LimbVigor: Boop  Hemolymph 38/100  left leg 4% dormant`.

Send the notes back and the plugin gets patched.

- [ ] **Reaches the menu.** `ready v1.25 — Widget setCaption + show LifeBar10 after orig` then `Main menu loaded`. Must NOT log `HUD created at title screen`. Must NOT hook MainBarGUI ctor `0x72C1E0`. Must NOT walk the Gui tree. Must NOT `setSize`. Must NOT `setVisible` on Root / MedicalPanel / parents / LifeBar1–9. Must NOT call `_getWidget`. Must NOT log the rejected v1.23 / v1.24 / `46d810a` ready strings.
- [ ] **Loads the save.** `LimbVigor: In-game` (or `In-game — ignoring stuck gameResetting`) then `player squad seen`. A −15 empty stump must get an LV part (`slotted LV Stump/Grown … (-15 empty socket)`) or a logged skip saying why. If it does not tick, the log must say **why**. Must NOT touch Character during title / save load. Must NOT crash after In-game.
- [ ] **Person-select shows Hemolymph on LifeBar10 (after Hunger).** Select a **person**. Whole HUD stays visible and in slots. Blood stays Blood. You must **see** Hemolymph on the new bottom row (not only a log line). Log every `setCaption` export. `setCapW=1` (Widget, not TextBox-only). After orig: `after show vis10=1`. Read-back `getCaption` is Hemolymph. `painted=1` only if vis=1 and Hemolymph/Vigor/Battle-heat. Door / box / chair: hide or clear LifeBar10* only — never touch LifeBar1. Send `RE_Kenshi_log.txt`.
- [ ] **HUD SEH does not kill growth.** If a HUD write excepts, growth / heartbeat keep ticking. Must NOT latch `medical row stopped` forever. Must NOT log 500+ empty panel walks. Must NOT die after one `medical tick SEH`.
- [ ] **Selected body, not the player pawn.** Select a **hired Hive** and a **hired Shek**. LifeBar10 is that body's Hemolymph / Battle-heat on screen. Blood stays Blood. I-key snap matches the selected body. Door / building: clear LifeBar10 only. Must NOT only paint the player character. Must NOT shift LifeBar1–9.
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
- [ ] **Grown is the limb.** At 100% the I-key slot should say `LV Grown …`, not −15 and not original flesh. We do not call `setLimb(ORIGINAL)`. Send the log either way. Boop's already-100% left leg should become Grown on load.
- [ ] **Save / load.** Grow to ~40%, save, quit, load. Numbers persist (LimbVigor.progress). No MyGUI create after In-game.
- [ ] **Squad.** Player + hired hive + hired shek, each selected I-key shows their own numbers.
- [ ] **No MyGUI crash.** No TitleScreen widget create. No in-game widget create. No MyGUI mouse delegates. No eventFrameStart. Dark UI must reach the menu and survive save load.

## Notes

Race / limb / bed / fed / combat / I-key text / log:
