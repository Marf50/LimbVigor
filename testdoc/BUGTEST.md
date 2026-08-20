# Bug-test sheet

Throwaway save. Tick the box. Write the last I-key tooltip line and anything in `RE_Kenshi_log.txt`.

**Where to look:** select a **character** after In-game. A **new row under Hunger** (LifeBar10) must sit **tight under Hunger** (Blood→Head gap) as a **Root child after MedicalPanel** (v1.33 coords, Depth=1 — not `LifeBar10Slot`, not last-child-of-Front) and show **Hemolymph** on the left (Datapanel setSize host W × Hunger H, never a 0x0 LifeBar9Datapanel copy) and a **number** in the right column (Value). Green fill matches hemolymph — not grey/dotted, not 2px. Blood stays Blood. LifeBar1–9 must sit **in their Dark UI slots** (`3b999b9` fractions of Back). Floor / Speed stay uncovered. Log: `setCapW=0` expected. `setCapISub=1`. Green `parentW` is 50–400. If LifeBar10 **and** LifeBar9 getW≤14, log `Green host fallback layout Hunger parentW=NNN`. After orig `vis10=1 visVal=1 visGrey=0`. `painted=1` only if vis=1 and Datapanel is Hemolymph/Vigor/Battle-heat and Value is a number and green w>4 and Grey is hidden and Datapanel is not 0x0 / caption empty. A Left Leg at **5 crippled** must log `left leg STUMP → budding nub attached hp=…` **or** `nub attach SEH skip` — never an unlogged crash. Must NOT Equip GROWN first. Must NOT `setLimb(ORIGINAL)`. Intact 75-HP arms stay 75. ESC pause must not crash (no `gui-epoch`). See testdoc/HUD.md.

Every ~15s the log prints `LimbVigor: Boop  Hemolymph 38/100  left leg 4% dormant`.

Send the notes back and the plugin gets patched.

- [ ] **Reaches the menu.** `ready v1.39 — HUD lifetime + LifeBar11 bind, bar stays, ESC close must live` then `Main menu loaded`. Must NOT log `HUD created at title screen`. Must NOT hook MainBarGUI ctor `0x72C1E0`. Must NOT walk the Gui tree. Must NOT `setSize` except LifeBar10Green / a 0-size LifeBar10Datapanel. Must NOT `setVisible` on Root / MedicalPanel / Back / Front / LifeBar1–9. Must NOT call `_getWidget`. Must NOT hunt Widget::setCaption. Must NOT log the rejected v1.23–v1.38 / `46d810a` ready strings. Must NOT contain `HemolymphStrip` or `LifeBar10Slot`. Must NOT call restore-on-stump / Equip GROWN first / setLimb ORIGINAL. Must NOT log `gui-epoch`. Must NOT log `restore-on-stump skipped … no write`. Must NOT skip paint because PausedPanel/Options/Settings vis=1. Must NOT hook ESC or the injected settings button. Must NOT findWidgetT OptionsPanel / odSettingsOpen. LifeBar10Datapanel is PanelEmpty, not a TextBox. LifeBar11 is a hidden 0-size bind stub, not a second Hemolymph row.
- [ ] **Loads the save.** `LimbVigor: In-game` (or `In-game — ignoring stuck gameResetting`) then `player squad seen`. A −15 empty stump must get an LV part or log `nub attach SEH skip` / why. Must NOT touch Character during title / save load. Must NOT crash after In-game.
- [ ] **Person-select shows Hemolymph on LifeBar10 (after Hunger).** Select a **person**. LifeBar1–9 sit in their Dark UI slots. LifeBar10 sits **tight under Hunger** as a Root child after MedicalPanel (v1.33 coords, Depth=1, parent Root — not LifeBar10Slot). Floor / Speed stay uncovered. **Hemolymph** is on the left (Datapanel setSize host W × Hunger H), **99** is in the right column. Green is a real bar — `setSize` to `(hemo/max)*parentW` when host is 50–400, not left at w=0. Log stays quiet (ready / nub attach / `HUD invalidate (widget died)` once / 15s heartbeat). Left Leg 5 must log `left leg STUMP → budding nub attached` **or** `nub attach SEH skip` (EquipWriteSeh actually runs). Must NOT log `restore-on-stump skipped … no write`. Must NOT Equip GROWN first, must NOT `setLimb(ORIGINAL)`. ESC / settings close must leave Kenshi alive — when MainPanel reload frees LifeBar10*, log `HUD invalidate (widget died)` and skip writes that tick, then re-resolve. Must NOT die with zero LimbVigor lines after `Injected settings button`. Must NOT log `HUD freeze` / `HUD thaw` (v1.38 name-guess is dead). 75-HP arms stay 75. Send `RE_Kenshi_log.txt`.
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
- [ ] **Grown is the limb.** Stages increment: STUMP nub first, then Budding → Forming → Knitting → Grown. At 100% the I-key slot should say `LV Grown …`, not −15 and not original flesh. We do not call `setLimb(ORIGINAL)`. Never Equip GROWN as the first write. Send the log either way.
- [ ] **Save / load.** Grow to ~40%, save, quit, load. Numbers persist (LimbVigor.progress). No MyGUI create after In-game.
- [ ] **Squad.** Player + hired hive + hired shek, each selected I-key shows their own numbers.
- [ ] **No MyGUI crash.** No TitleScreen widget create. No in-game widget create. No MyGUI mouse delegates. No eventFrameStart. Dark UI must reach the menu and survive save load.

## Notes

Race / limb / bed / fed / combat / I-key text / log:
