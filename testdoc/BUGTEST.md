# Bug-test sheet

Throwaway save. Tick the box. Write the last I-key tooltip line and anything in `RE_Kenshi_log.txt`.

**Where to look:** select a **character** after In-game. Blood row should read **Hemolymph / Vigor / Battle-heat** if the hunt found LifeBar1. Door / box / chair must look like a door (Blood/Oil back, no Hemolymph). Log MainBar `proven=1`, member hunt, `_getWidget` this=MainBar / BaseLayout+0x30, `setCaption` bind, and `painted=1` or `painted=0`. If `GUI probe SEH` appears, `_getWidget` must **not** run again. Must NOT `setSize` / resize MedicalPanel / shift LifeBar2–9. Must NOT treat +0x40 as a C string. Must NOT compute MainBar = medicalPanel-0x188. Must NOT call ctor `0x72C1E0`. Must NOT walk the Gui tree. Must NOT paint Goal/State. See testdoc/HUD.md.

Every ~15s the log prints `LimbVigor: Boop  Hemolymph 38/100  left leg 4% dormant`.

Send the notes back and the plugin gets patched.

- [ ] **Reaches the menu.** `ready v1.21 — hunt MainBar widgets + setCaption Hemolymph if found, no Datapanel` then `Main menu loaded`. Must NOT log `HUD created at title screen` or `Blood HUD created after in-game`. Must NOT die after title create / at ~12s with `Log manager destructor`. Must NOT hook MainBarGUI ctor `0x72C1E0` / changeFontSize / `_NV_update` / `_NV_setObject`. Must NOT walk the Gui tree. Must NOT `setSize`.
- [ ] **Loads the save.** `LimbVigor: In-game` (or `In-game — ignoring stuck gameResetting`) then `player squad seen`. A −15 empty stump must get an LV part (`slotted LV Stump/Grown … (-15 empty socket)`) or a logged skip saying why. If it does not tick, the log must say **why**. Must NOT touch Character during title / save load. Must NOT crash after In-game.
- [ ] **Person-select does not kill the process.** Select a **person**. Game stays up. Blood row caption is Hemolymph / Vigor / Battle-heat if a widget was found (`painted=1 setCaption`). Must NOT retry `_getWidget` after `GUI probe SEH`. Door / box / chair: Blood/Oil back, no Hemolymph. Send `RE_Kenshi_log.txt`.
- [ ] **HUD SEH does not kill growth.** If a HUD write excepts, growth / heartbeat keep ticking. Must NOT latch `medical row stopped` forever. Must NOT log 500+ empty panel walks. Must NOT die after one `medical tick SEH`.
- [ ] **Selected body, not the player pawn.** Select a **hired Hive** and a **hired Shek**. Blood row caption is that body's Hemolymph / Battle-heat. I-key snap matches the selected body. Door / building: Blood/Oil back. Must NOT only paint the player character. Second HUD row is not in v1.21.
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
