# HUD notes (living)

Proven facts. Update when a playtest proves a new one.

## Layout names (Dark UI workshop 1200632417 + LimbVigor override)

- Blood is **LifeBar1** (first bar). LifeBar2..9 = Head / Stomach / Chest / arms / legs / Hunger.
- Dark UI omitted **LifeBar10**. MyGUI `LifeBar10 not found` is Dark UI omitting a slot MainBar already `assignWidget`-binds.
- LimbVigor ships `kenshi_mod/gui/layout/Kenshi_MainPanel.layout` started from workshop **1200632417** Dark UI. **MedicalPanel y STAYS** `0.70473849555969238`. x/w/h stay byte-exact. Back / Front / LifeBar1–9 stay byte-exact `4ea66b2` / `3b999b9`. Front stays `0 0 0.99665552377700806 1.0030864477157593` `Depth=0`. **v1.34 `LifeBar10Slot` BottomSkin 1-row made getW=2 / invisible** — do **not** reintroduce it. **v1.35:** LifeBar10 + Value + Datapanel + Tooltip are **Root children after MedicalPanel** at v1.33 coords (`LifeBar10` `0.14218749750845783 0.9797385139738242 0.12395832622918707 0.02129629746523392`, Depth=1). Datapanel stays PanelEmpty `0 0.1304347813129425 0.99579828977584839 0.73913043737411499` as a child of LifeBar10. Hunger→Hemolymph gap stays 0.000926. Size Green from a host whose getW is 50–400. If LifeBar10 **and** LifeBar9 getW≤14, use layout Hunger pixels (`Green host fallback layout Hunger parentW=NNN`) — never size from 2. Do **not** copy LifeBar9Datapanel if 0x0; setSize host W × Hunger H, then ISub Hemolymph. Do **not** stretch the 9-row Back/Front. No `HemolymphStrip`. MedicalPanel stays left of Floor / Speed. Touch no non-medical chrome.
- SHA `2bc122a` (v1.23 full-layout drift: MedicalPanel `0.3→0.333`, children `position_real` so every bar grew ~11%) is **rejected**. SHA `46d810a` (LifeBar1 overwrite) is also rejected.
- LifeBar1 / LifeBar10 are **Widget** (PanelEmpty), not a TextBox. LifeBarNValue is the digit TextBox.
- LifeBarN Green / fill skins: do not setCaption them.
- Captions (Blood/Head/…) are written by `MedicalSystem::getMedicalGUIData` **every tick**. That is why LifeBar1 overwrite lost to Blood.

## Pointers

- MainBarGUI* = ForgottenGUI `gui+0x10`. Prove with `*(bar+0x188) == medicalPanel`. CHECK, not `medicalPanel - 0x188`.
- Root is at **MainBar+0x8**.
- `+0x188` is a pointer **to** MedicalDatapanel. Not DatapanelGUI. Do not `setLineProgress` on it.

## Prefix is real (v1.21)

- BaseLayout prefix @ +0x40 is a real MSVC `std::string` (v1.21: size=22 cap=31 heap=1).
- Value `'0,000,000,048,7F5,0E0_'` with MainBar `0x487F50B0`. That is **hex(bar+0x30) grouped in threes + `_`**. `0x487F50E0 == bar+0x30`.
- v1.19 `'0,000,000,048,8D2,510_'` / v1.22 `'0,000,000,048,722,1C0_'` are the same pattern. Not C-string garbage.
- getName on Root was `'0,000,000,048,7F5,0E0_Root'` — prefix + widget name.
- **Do not skip 3-arg findWidgetT because the prefix looks ugly.** That skip was the v1.21 miss.

## Find path (v1.22 proved the prefix; v1.23 finds LifeBar10)

- 3-arg `findWidgetT(name, prefix, throw=false)` found LifeBar1 in v1.22. Same path, **different names**: `LifeBar10`, `LifeBar10Datapanel`, `LifeBar10Value`. Also `Widget::findWidget` on Root (`bar+0x8`).
- **Do not hunt again.** Cache those three. **Do not call `_getWidget` RVA `0x723780`.**
- Never cache or `setCaption` LifeBar1 / LifeBar1Datapanel / LifeBar1Value.

## Write path (v1.27 — Datapanel label + numeric Value + Green fill)

- v1.22 find+write logged `painted=1` but Dylan saw only the vanilla HUD. Two causes:
  1. Bound `?setCaption@TextBox@MyGUI@@UEAAXAEBVUString@2@@Z` and called it on LifeBar1 (Widget). Wrong vtable. No pixels.
  2. Paint ran once. `getMedicalGUIData` writes Blood back every tick after that.
- CoS rejects another LifeBar1 overwrite. New row only: **LifeBar10 after Hunger.**
- Bind `Widget::setCaption` — prefer exact `?setCaption@Widget@MyGUI@@QEAAXAEBVUString@2@@Z`, keep UEAAX. Call that on LifeBar10 and LifeBar10Datapanel.
- `TextBox::setCaption` only on LifeBar10Value (the digit). Never TextBox-on-Widget. Never `setCaption` on LifeBar1.
- v1.23 playtest (Dylan): layout loaded (first real HUD change) but bars overflowed. Find succeeded: LifeBar10 `vis=0`, LifeBar10Value `vis=0`, LifeBar10Datapanel `vis=1`, orig on LifeBar10 was already `Blood`. `setCapW=0`. `TextBox::setCaption` on LifeBar10Value did not stick (`getCaption` empty).
- Bind `Widget::setCaption` for real — exact `?setCaption@Widget@MyGUI@@QEAAXAEBVUString@2@@Z` and UEAAX. Log `setCapW`. Retry if missing (do not latch a failed dump). LifeBar10 / LifeBar10Datapanel **must** use Widget::setCaption, not TextBox.
- v1.24 playtest (Dylan): chrome OK (layout sha256 `d7a31296…`). LifeBar10 FOUND `vis=0`. `setCapW=0` every tick (TextBox-only or QEAAX miss). TextBox on Value stayed empty. `setVis` bound but LifeBar10 stayed `vis=0` — game hides unused LifeBar10 **after** orig. Show must run after orig every selected-person tick.
- Dump **every** MyGUI `setCaption` export (log each decorated name). Bind `?setCaption@Widget@MyGUI@@QEAAXAEBVUString@2@@Z` (prefer), UEAAX, and `ISubWidgetText::setCaption` if present. **Do not** treat a TextBox-only bind as success. If Widget::setCaption is missing, retry the dump (do not latch `setCapW=0`). Log `setCapW` / `setCapText` / `setCapISub` / `setVis` every bind.
- After orig every selected-person tick: `setVisible(true)` on **LifeBar10, LifeBar10Datapanel, LifeBar10Value, LifeBar10Green** only (getName ends with that exact token). Never Root / MedicalPanel / Back / Front / LifeBar1–9 / Squad / Name / parents. Log vis immediately after show.
- Then Widget::setCaption Hemolymph on LifeBar10 / LifeBar10Datapanel. TextBox or ISubWidgetText on LifeBar10Value only. Do not setCaption Green / fill skins.
- `painted=1` only if LifeBar10 `vis=1` **and** getCaption is Hemolymph / Vigor / Battle-heat. `vis=0` or empty / Blood is a fail.
- Door / box / chair: `setVisible(false)` + clear those LifeBar10* only. Never touch LifeBar1. Blood stays Blood.
- v1.24 layout sha256 `d7a31296…` was Dark UI exact + LifeBar10 tucked. v1.28 Root-level `HemolymphStrip` is **rejected**. v1.29 LifeBar10 lives in MedicalPanel under Back. No LifeBar1 `setCaption`. No runtime `createWidget`.
- v1.25 playtest (Dylan, Boop): chrome OK. LifeBar10 FOUND. After show `vis10=1 visData=1 visVal=1 visGreen=1` every tick. `painted=0` because caption empty. **Widget::setCaption is not in MyGUIEngine exports** (12 names: EditBox, EditText, ISubWidgetText, ListScrollBar, MenuItem, MultiListItem, MultiSlider, Slider, TabItem, TextBox, Window). `setCapW=0` forever. `setCapISub=1` was bound and never used for the key write. LifeBar10 was ON but off the bottom of the screen (MedicalPanel y+h = 1.018).
- v1.26: slide MedicalPanel y `0.71852→0.68852` only. Write **ISubWidgetText::setCaption** on LifeBar10Datapanel and/or `getSubWidgetText(LifeBar10)`. Bind EditText::setCaption if the text child is EditText. `setCapW=0` is expected — do not hunt Widget::setCaption. TextBox-on-Value is not `painted=1`. Show-after-orig stays.
- v1.26 playtest (Dylan, Boop, paused): **first real extra bar**. Wrong on screen: LifeBar10 under the grey chrome (Back skin stops at Hunger). Word **Hemolymph** in the number column (`…molym` under Hunger’s 145) — that was LifeBar10Value. Bar fill empty — LifeBar10Green never sized.
- v1.27: grow MedicalPanel + Back, rescale 1–10 / Value / Tooltip (pixel-true 1–9). Hemolymph on **LifeBar10Datapanel only**. Value is the **number** only. `setSize`/`setCoord` LifeBar10Green to `(hemo/max)*parentW`. `painted=1` if vis10=1 and Datapanel is Hemolymph/Vigor/Battle-heat and Value is a number and green w>0.
- v1.27 playtest (Dylan): 10th row on the grey plate after Hunger, number 99 on Value. Then two more fails: **LifeBar1–9 not in their slots** (outlines float over the middle of some bars) and **LifeBar10 chopped** with a dashed line through it. Cause: stretching the Dark UI 9-bar skin (`Kenshi_HealthPanelBottomSkin` / Front) to cover LifeBar10. Also: no Hemolymph label on the bar; fill is a **1–2px green sliver**.
- v1.28: **revert** 1–9 to `3b999b9` and put Hemolymph on a Root-level `HemolymphStrip`. Dylan rejected: separate bar, no 10th row on the plate.
- v1.28 write FAIL: `getW=0 getCoord=0`. parentW/fillW/green w / Datapanel coord were **~1.1e9** (widget pointers used as widths). ISub and Value `getCaption` stayed `''`. `painted=0`.
- v1.28 growth FAIL: `Boop Hemolymph 100/100 no stump` + `parts SEH — growth numbers kept`. Shot: Left Leg **5 crippled / cut-off**, Right Leg 23, arms 75/75. “no stump” was a miss. Left Leg 5 is a stump. Arms stay 75.
- v1.29: LifeBar10 in MedicalPanel under Back. Green bind is a **real int** `getWidth`. Stump detector + cheap per-limb SEH. **75-HP arms were a false alarm.**
- v1.29 playtest: HUD visible in-stack (first time). Then **CRASH**. Label not centered. ISub `getCaption` `''`. Datapanel coord **0x0** (LifeBar10Datapanel was a TextBox; LifeBar9 is PanelEmpty). Green FAIL `getW=14`. Left leg STUMP 5/75, then `SEH site=restore` collapsed it to **5/5 whole**. Restore-on-stump is the crash.
- v1.30: **do not restore a stump** (no Equip GROWN / setLimb / validateHealth on a live nub). LifeBar10Datapanel = **PanelEmpty**, same coords as LifeBar9Datapanel. ISub via getSubWidgetText. Measure **LifeBar10 host after show**; wait if width < 50. `painted=0` if Datapanel 0x0 or getW<50 or parentW>1000 or green w≤4. Ready: `ready v1.30 — no restore-on-stump + PanelEmpty Datapanel + pixel Green host`.
- v1.30 playtest FAIL: HUD still grey/dotted (stun skin left visible). Left Leg **numbers-only** (no in-game HP). Settings-close crash. Empty space above/below Hemolymph (Hunger→bar gap ≈0.027 screen).
- v1.31: tight stack (3b999b9 plate + LifeBar10 tucked after Front). Hide Grey*; Green from host 50–400. Flesh write; no Equip GROWN. Settings-close dropped **LifeBar10 only**. Ready: `ready v1.31 — tight stack + green fill + stump HP + settings-close safe`.
- v1.31 playtest FAIL: settings-close still died (`Injected settings button` → log destructor, **zero** LimbVigor lines). Sibling-after-Front left Hemolymph behind chrome (Front Depth=0). Left stump `5.0 -> 5.0 / 5.0` (max clamped). Green `getW=14` forever. Datapanel 0x0.
- v1.32: panel down 1 bar. LifeBar10* last children of Front. Settings-close must drop **ALL** stale HUD pointers (LifeBar10*, Grey*, LifeBar9 width cache, Root, MedicalPanel, Front, Back) and skip paint/setCaption/setSize/setVisible/stump until re-found after options. Stump writes flesh **and** `_maxHealth` toward the real max (75), not freeze 5/5. Green uses LifeBar9 width if LifeBar10 getW≤14. Ready: `ready v1.32 — panel down 1 bar + LifeBar10 on top + settings-close safe + stump max + green host`.
- v1.32 playtest FAIL: last-child-of-Front still under chrome (`LifeBar10Value vis=0`, Green WAIT getW=13, LifeBar9 cached but fallback never applied, Datapanel 0x0). Stump jumped `5.0/5.0 -> 75.0/75.0` at progress=100 (flesh+max, no budding/knitting). **LvHudWatchGui epoch-drop crashed ESC pause** (destructor 177.4 / Crash detected 178.1). No `Injected settings button` — that was options-close.
- v1.33: nudge panel up 0.006. LifeBar10* parent Root after MedicalPanel, Depth=1. **Use** LifeBar9 getW when LifeBar10 getW≤14 (log `Green host fallback LifeBar9 parentW=NNN (LifeBar10 getW=13)`). Stump flesh/max **+1..+4 HP per tick** (5→~8→~12), stay STUMP. **No** LvHudWatchGui epoch-drop. **No** options/ESC/pause hooks. **No** cache wipe on MainBar pointer change. SEH-skip a dead widget write; leave the rest of the cache. Ready: `ready v1.33 — nudge panel up + Hemolymph visible on top + staged stump + no ESC/pause hooks`.
- v1.33 playtest FAIL (Dylan): Hemolymph had **no left label** (LifeBar10Datapanel vis=1 but **0x0**, getCaption empty). Bar was a **flat fill** — Root-after-MedicalPanel sat outside the 9-slot BottomSkin. Left stump gained HP numbers (`5/5 → 8/8 → 11/9`) and stayed `kind=stump` / fully missing / Crippled. **HP-on-stump is not a limb.**
- v1.34: 1-row `LifeBar10Slot` chrome (Hunger-height BottomSkin under Back). LifeBar10 parent = LifeBar10Slot. Datapanel sized from LifeBar9Datapanel pixels; ISub must read back Hemolymph. `painted=0` if grey vis, green w≤4, or Datapanel still 0x0 / caption empty. Depth=2 on LifeBar10 + Datapanel + Value so they paint on top of the chrome. **Nub = slotted growth part** (`LvEquipGrowthPart` STUMP → BUD → FORMING → KNITTING). Log `left leg STUMP → budding nub attached hp=…`. Never Equip GROWN first. Never `setLimb(ORIGINAL)`. Legs-first. Arms 75 intact stay. No epoch-drop. Ready: `ready v1.34 — Hemolymph label + vanilla bar chrome + real nub limb stages`.
- v1.34 playtest FAIL (Dylan): bar **invisible**, no text. LifeBar10 parent=LifeBar10Slot, vis10=1, Green WAIT getW=2, LifeBar9 fallback parentW=2, Datapanel size from LifeBar9Datapanel **0x0**, caption empty, `painted=0`. Left leg still STUMP 11.0/5.0, no `nub attached` line. ~10s after select: HUD paint → destructor → Crash detected. EquipGrowthPart must SEH-log skip, never unlogged crash.
- v1.35: **delete LifeBar10Slot**. LifeBar10 parent Root after MedicalPanel (v1.33 coords, Depth=1). Green layout-Hunger fallback when both getW≤14. Datapanel setSize host W × Hunger H (not 0x0 copy). Equip write (createItem / SlotPart / setLimb REPLACED / updateStats) is SEH-wrapped: `left leg STUMP → budding nub attached hp=…` or `nub attach SEH skip`, then skip that write. No epoch-drop. Ready: `ready v1.35 — bar visible like 1.33 + Hemolymph label + SEH nub attach`.
- v1.35 playtest FAIL (Dylan): cannot open settings. `Injected settings button` → destructor → Crash. Cause: ISub/setSize/setVisible on cached LifeBar10* after options teardown. Left leg 11.0/5.0 STUMP; `restore-on-stump skipped … nub keeps ticking, no write` blocked EquipWriteSeh (zero `nub attached` / `nub attach SEH skip`). parentW=272 good; green w=0; Datapanel 0x0.
- v1.36: **SEH every MyGUI HUD call** (setCaption / setSize / setVisible / getW / findWidget). On exception: null that widget’s cache, skip paint this tick, log `HUD SEH skip (options teardown)` once. Skip paint while read-only findWidget sees Paused or Options (not an ESC hook, not WatchGui). After close, re-find and paint. `restore-on-stump skipped` removed — that path now calls LvSyncOneLimb / EquipWriteSeh (STUMP first). Green width = (hemo/max)*parentW (272). Datapanel setSize host W × Hunger H. LifeBar10Slot stays gone. Ready: `ready v1.36 — ESC/settings close lives + nub attach actually runs`.
- v1.36 playtest FAIL (Dylan): opening RE_Kenshi_log in-game crawls (per-tick dump panel / after show / Datapanel 0x0 / parentW / hunt / findWidgetT). Bar gone — `lvHudMenuOpen` skipped paint because PausedPanel/Options/Settings can exist vis=1 while not paused.
- v1.37: **delete lvHudMenuOpen / skip-paint-because-Paused-or-Options.** Only skip HUD writes inside LV_TRY after a real exception, then re-find next tick. Per-tick HUD logs gated: no dump-panel walks, no findWidget traces, no `static int n < 8/12/16` paint spam. Keep ready once, nub attach / nub attach SEH skip, `HUD SEH skip (options teardown)` once, 15s heartbeat. Green `setSize` to `(hemo/max)*parentW` when 50–400. LifeBar10Slot stays gone. Ready: `ready v1.37 — quiet log + bar visible like 1.33 + ESC close still lives`.

## Landmines

No runtime `createWidget`. No HemolymphStrip. No `LifeBar10Slot`. No MainBar ctor `0x72C1E0`. No `eventFrameStart`. No TitleScreen MyGUI. No `ForgottenGUI::changeFontSize`. No DatapanelGUI `_NV_update` / `_NV_setObject`. No `GetRealAddress` on virtuals. No Gui tree walk. No Goal/State paint. No WindowCX. No `setSize` except LifeBar10Green / a 0-size LifeBar10Datapanel. No `_getWidget`. No `setVisible` on Root / MedicalPanel / Back / Front / LifeBar1–9. No Datapanel / `setLineProgress` HUD path. No unlogged Equip throw. No ESC/pause/options hooks. No epoch-drop. No `restore-on-stump skipped … no write`. No PausedPanel/Options vis=1 skip-paint. No per-tick HUD log spam (once-per-resolve / 15s / on-change only).

## Version trail

- v1.16–v1.18: bind / tree walk / unprefixed find all null.
- v1.19: `_getWidget` person-select crash. Retry after probe SEH killed the process.
- v1.20: crash lock PASS. Skipped `_getWidget`. Unprefixed find all null. `painted=0`.
- v1.21: prefix dumped as "ugly" and findWidgetT skipped. Then `_getWidget`/probe SEH. `painted=0`. **Whole HUD hid.** Prefix was real.
- v1.22: prefixed find+write succeeded (`painted=1` Hemolymph on LifeBar1). TextBox::setCaption on Widget + once-only lost to Blood overwrite. Vanilla HUD only.
- v1.23 **rejected** (`46d810a`): Widget::setCaption every tick on LifeBar1. CoS / Dylan: Blood stays Blood. Do not ship that ready string.
- v1.23 (`2bc122a`): LifeBar10 after Hunger, but grew MedicalPanel and drifted the whole layout. Children are `position_real` so parent scale changes pixel size. Bars larger, Blood off the top, Hunger off the bottom, name plate / chrome shifted. Find: LifeBar10 `vis=0`, `setCapW=0`, TextBox write empty.
- v1.24: Dark UI exact layout (e062b6d, sha256 `d7a31296…`). Chrome OK. `setCapW=0`, LifeBar10 stayed `vis=0`. Ready was `ready v1.24 — Dark UI exact + LifeBar10 tucked, no parent scale`.
- v1.25: same layout. Dump every setCaption export. Bind Widget QEAAX/UEAAX + ISub. Show LifeBar10 / Datapanel / Value / Green after orig. `painted=1` only if vis=1 and Hemolymph/Vigor/Battle-heat. Ready: `ready v1.25 — Widget setCaption + show LifeBar10 after orig`.
- v1.25 playtest: show worked (`vis10=1`). Widget::setCaption **not exported**. ISub bound, unused for the write. LifeBar10 off the bottom. Ready v1.25 rejected as a write hunt.
- v1.26: MedicalPanel y `0.71852→0.68852` only (nothing else). ISub setCaption on Datapanel / `getSubWidgetText(LifeBar10)`. EditText if the child is EditText. `setCapW=0` expected. Ready: `ready v1.26 — MedicalPanel up + ISub setCaption Hemolymph`.
- v1.26 playtest: first extra bar landed. Under chrome, word on Value, Green empty.
- v1.27: MedicalPanel grow + rescale 1–10. Hemolymph on Datapanel. Value numeric. Green fill. Ready: `ready v1.27 — MedicalPanel grow + Hemolymph on Datapanel + Green fill`.
- v1.27 playtest: 10th row on the plate, number 99. Then 1–9 slot drift + LifeBar10 chopped by the 9-bar dashed cap. Blank label. 1px sliver (getWidth=0 clamped to 1, or fraction-as-pixels). Stretching the 9-bar skin is rejected.
- v1.28: revert 1–9 to `3b999b9`. New Hemolymph strip. Ready: `ready v1.28 — revert 1–9 + new Hemolymph strip + pixel Green`.
- v1.28 playtest FAIL: separate strip (no 10th row). Caption `''`. parentW ~1.1e9. `no stump` while Left Leg 5 crippled. parts SEH.
- v1.29: LifeBar10 in MedicalPanel under Back. Real pixel getWidth. Hemolymph on Datapanel. Stump = official / crippled / cut-off nub. Per-limb parts SEH (cheap; not a parts rewrite). **75-HP arms were a false alarm** — no intact-limb heal. Ready: `ready v1.29 — LifeBar10 in MedicalPanel + pixel Green + stump growth`.
- v1.29 playtest FAIL: bar visible, then crash. TextBox Datapanel 0x0 + off-center label. getW=14. Restore-on-stump turned Left Leg 5/75 into 5/5 whole.
- v1.30: no restore-on-stump. PanelEmpty Datapanel (match LifeBar9). Green measures LifeBar10 host after show. Ready: `ready v1.30 — no restore-on-stump + PanelEmpty Datapanel + pixel Green host`.
- v1.30 playtest FAIL: grey/dotted bar; numbers-only stump; settings-close crash; Hunger–Hemolymph gap.
- v1.31: tight stack + hide stun skins + flesh HP + cache drop on teardown. Ready: `ready v1.31 — tight stack + green fill + stump HP + settings-close safe`.
- v1.31 playtest FAIL: settings-close still died (no LimbVigor drop log). Hemolymph behind Front chrome. Stump max froze 5/5. Green getW=14.
- v1.32: panel down 1 bar + LifeBar10 last children of Front + drop ALL stale HUD pointers + stump max climbs + LifeBar9 green host fallback. Ready: `ready v1.32 — panel down 1 bar + LifeBar10 on top + settings-close safe + stump max + green host`.
- v1.32 playtest FAIL: last-child-of-Front failed; epoch-drop crashed ESC pause; stump jumped 5→75.
- v1.33: panel y `0.70473849555969238`. LifeBar10 parent Root after MedicalPanel, Depth=1. LifeBar9 fallback actually used. Stump +1..+4/tick. No ESC/pause hooks. Ready: `ready v1.33 — nudge panel up + Hemolymph visible on top + staged stump + no ESC/pause hooks`.
- v1.34: MedicalPanel y unchanged. `LifeBar10Slot` 1-row chrome. LifeBar10 parent LifeBar10Slot. Datapanel from LifeBar9Datapanel pixels. Nub is a slotted growth part and stages. Ready: `ready v1.34 — Hemolymph label + vanilla bar chrome + real nub limb stages`.
- v1.35: `LifeBar10Slot` gone. LifeBar10 parent Root, v1.33 coords. Green layout Hunger when both getW≤14. Datapanel not 0x0 copy. Equip SEH-logs skip. Ready: `ready v1.35 — bar visible like 1.33 + Hemolymph label + SEH nub attach`.
- v1.36: options teardown SEH every HUD MyGUI call. Paused/Options skip-paint. EquipWriteSeh actually runs on a STUMP. Ready: `ready v1.36 — ESC/settings close lives + nub attach actually runs`.
- v1.37: PausedPanel vis=1 skip-paint hid the bar; only skip after a real HUD SEH. Per-tick logs lag the game; once-per-resolve / 15s / on-change only. Ready: `ready v1.37 — quiet log + bar visible like 1.33 + ESC close still lives`.
