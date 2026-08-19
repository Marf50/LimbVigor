# HUD notes (living)

Proven facts. Update when a playtest proves a new one.

## Layout names (Dark UI workshop 1200632417 + LimbVigor override)

- Blood is **LifeBar1** (first bar). LifeBar2..9 = Head / Stomach / Chest / arms / legs / Hunger.
- Dark UI omitted **LifeBar10**. MyGUI `LifeBar10 not found` is Dark UI omitting a slot MainBar already `assignWidget`-binds.
- LimbVigor ships `kenshi_mod/gui/layout/Kenshi_MainPanel.layout` started from workshop **1200632417** Dark UI. **v1.28:** LifeBar1–9 / Back / Front / Values / Tooltips are **v1.26 (`3b999b9`) coords** (unstretched 9-slot plate). MedicalPanel **h stays 0.300**. y slides further up (`0.68852→0.66722`) so a new one-row `HemolymphStrip` (`Kenshi_FloatingPanelSkin`, not BottomSkin/TopSkin) fits under Back. LifeBar10 + Datapanel + Green + Value + Tooltip live on that strip only. Do **not** stretch `Kenshi_HealthPanelBottomSkin` / TopSkin to cover a 10th row (v1.27). Do not grow a parent without rescaling children (v1.23). Touch no non-medical chrome.
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
- After orig every selected-person tick: `setVisible(true)` on **LifeBar10, LifeBar10Datapanel, LifeBar10Value, LifeBar10Green, HemolymphStrip** only (getName ends with that exact token). Never Root / MedicalPanel / Back / Front / LifeBar1–9 / Squad / Name / parents. Log vis immediately after show.
- Then Widget::setCaption Hemolymph on LifeBar10 / LifeBar10Datapanel. TextBox or ISubWidgetText on LifeBar10Value only. Do not setCaption Green / fill skins.
- `painted=1` only if LifeBar10 `vis=1` **and** getCaption is Hemolymph / Vigor / Battle-heat. `vis=0` or empty / Blood is a fail.
- Door / box / chair: `setVisible(false)` + clear those LifeBar10* / HemolymphStrip only. Never touch LifeBar1. Blood stays Blood.
- v1.24 layout sha256 `d7a31296…` was Dark UI exact + LifeBar10 tucked. v1.28 is a **medical-only** recut: revert 1–9 to that 9-bar geometry, add HemolymphStrip. No MedicalPanel h grow. No LifeBar1 `setCaption`. Layout XML for the new strip is OK; no runtime `createWidget`.
- v1.25 playtest (Dylan, Boop): chrome OK. LifeBar10 FOUND. After show `vis10=1 visData=1 visVal=1 visGreen=1` every tick. `painted=0` because caption empty. **Widget::setCaption is not in MyGUIEngine exports** (12 names: EditBox, EditText, ISubWidgetText, ListScrollBar, MenuItem, MultiListItem, MultiSlider, Slider, TabItem, TextBox, Window). `setCapW=0` forever. `setCapISub=1` was bound and never used for the key write. LifeBar10 was ON but off the bottom of the screen (MedicalPanel y+h = 1.018).
- v1.26: slide MedicalPanel y `0.71852→0.68852` only. Write **ISubWidgetText::setCaption** on LifeBar10Datapanel and/or `getSubWidgetText(LifeBar10)`. Bind EditText::setCaption if the text child is EditText. `setCapW=0` is expected — do not hunt Widget::setCaption. TextBox-on-Value is not `painted=1`. Show-after-orig stays.
- v1.26 playtest (Dylan, Boop, paused): **first real extra bar**. Wrong on screen: LifeBar10 under the grey chrome (Back skin stops at Hunger). Word **Hemolymph** in the number column (`…molym` under Hunger’s 145) — that was LifeBar10Value. Bar fill empty — LifeBar10Green never sized.
- v1.27: grow MedicalPanel + Back, rescale 1–10 / Value / Tooltip (pixel-true 1–9). Hemolymph on **LifeBar10Datapanel only**. Value is the **number** only. `setSize`/`setCoord` LifeBar10Green to `(hemo/max)*parentW`. `painted=1` if vis10=1 and Datapanel is Hemolymph/Vigor/Battle-heat and Value is a number and green w>0.
- v1.27 playtest (Dylan): 10th row on the grey plate after Hunger, number 99 on Value. Then two more fails: **LifeBar1–9 not in their slots** (outlines float over the middle of some bars) and **LifeBar10 chopped** with a dashed line through it. Cause: stretching the Dark UI 9-bar skin (`Kenshi_HealthPanelBottomSkin` / Front) to cover LifeBar10. Also: no Hemolymph label on the bar; fill is a **1–2px green sliver**.
- v1.28: **revert** MedicalPanel / Back / Front / LifeBar1–9 / Value / Tooltip to `3b999b9`. Do not grow MedicalPanel h. Do not stretch Back/Front. New `HemolymphStrip` sibling of MedicalPanel (plain `Kenshi_FloatingPanelSkin`). LifeBar10* on that strip only. Green fill uses LifeBar10 **pixel** width (`getWidth` / `getCoord` / `getAbsoluteCoord` / `getParentSize`). `w = (hemo/max)*parentPixelW`. Reject `getWidth=0`, fraction-as-int (`parentW < 32`), and the 1px clamp. Log `parentW` / `fillW` / green w. `painted=1` only if green w is a real bar (not a sliver): if `hemo/max >= 0.5`, green w ≥ ~50% of parentW. Hemolymph on Datapanel only; if caption reads Hemolymph but the bar is blank, log vis+coord and show/size/`setDepth` that widget. Datapanel XML is `0 0.13 0.996 0.739`, same as Blood.

## Landmines

No runtime `createWidget` (layout XML for HemolymphStrip is OK). No MainBar ctor `0x72C1E0`. No `eventFrameStart`. No TitleScreen MyGUI. No `ForgottenGUI::changeFontSize`. No DatapanelGUI `_NV_update` / `_NV_setObject`. No `GetRealAddress` on virtuals. No Gui tree walk. No Goal/State paint. No WindowCX. No `setSize` except LifeBar10Green / a 0-size LifeBar10Datapanel. No `_getWidget`. No `setVisible` on Root / MedicalPanel / Back / Front / LifeBar1–9. No Datapanel / `setLineProgress` HUD path.

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
- v1.28: revert 1–9 to `3b999b9`. New Hemolymph strip. Pixel Green fill + Hemolymph on Datapanel (runtime show/size/z if blank). Ready: `ready v1.28 — revert 1–9 + new Hemolymph strip + pixel Green`.
