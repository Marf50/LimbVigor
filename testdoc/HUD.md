# HUD notes (living)

Proven facts. Update when a playtest proves a new one.

## Layout names (Dark UI workshop 1200632417 + LimbVigor override)

- Blood is **LifeBar1** (first bar). LifeBar2..9 = Head / Stomach / Chest / arms / legs / Hunger.
- Dark UI omitted **LifeBar10**. MyGUI `LifeBar10 not found` is Dark UI omitting a slot MainBar already `assignWidget`-binds.
- LimbVigor ships `kenshi_mod/gui/layout/Kenshi_MainPanel.layout` (RE_Kenshi override, started from Dark UI). It inserts **LifeBar10 after Hunger / LifeBar9**. MedicalPanel / MedicalPanel_Back grow one row. **Do not shift LifeBar1–9.** Blood stays Blood.
- SHA `46d810a` (LifeBar1 overwrite / `Widget::setCaption` on Blood) is **rejected**. Do not ship that ready string.
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

## Write path (v1.23 — new row, not Blood overwrite)

- v1.22 find+write logged `painted=1` but Dylan saw only the vanilla HUD. Two causes:
  1. Bound `?setCaption@TextBox@MyGUI@@UEAAXAEBVUString@2@@Z` and called it on LifeBar1 (Widget). Wrong vtable. No pixels.
  2. Paint ran once. `getMedicalGUIData` writes Blood back every tick after that.
- CoS rejects another LifeBar1 overwrite. New row only: **LifeBar10 after Hunger.**
- Bind `Widget::setCaption` — prefer exact `?setCaption@Widget@MyGUI@@QEAAXAEBVUString@2@@Z`, keep UEAAX. Call that on LifeBar10 and LifeBar10Datapanel.
- `TextBox::setCaption` only on LifeBar10Value (the digit). Never TextBox-on-Widget. Never `setCaption` on LifeBar1.
- Write **after orig** every selected-person tick. Read back `getCaption`. `painted=1` only if it is Hemolymph / Vigor / Battle-heat. Still Blood is a fail (wrong dest).
- Door / box / chair: **hide or clear LifeBar10 only.** Never touch LifeBar1. Blood stays Blood.
- No MedicalPanel runtime `setSize`. No LifeBar1–9 shift.

## Landmines

No `createWidget`. No MainBar ctor `0x72C1E0`. No `eventFrameStart`. No TitleScreen MyGUI. No `ForgottenGUI::changeFontSize`. No DatapanelGUI `_NV_update` / `_NV_setObject`. No `GetRealAddress` on virtuals. No Gui tree walk. No Goal/State paint. No WindowCX. No `setSize`. No `_getWidget`. No `setVisible` on Root / parents. No Datapanel / `setLineProgress` HUD path.

## Version trail

- v1.16–v1.18: bind / tree walk / unprefixed find all null.
- v1.19: `_getWidget` person-select crash. Retry after probe SEH killed the process.
- v1.20: crash lock PASS. Skipped `_getWidget`. Unprefixed find all null. `painted=0`.
- v1.21: prefix dumped as "ugly" and findWidgetT skipped. Then `_getWidget`/probe SEH. `painted=0`. **Whole HUD hid.** Prefix was real.
- v1.22: prefixed find+write succeeded (`painted=1` Hemolymph on LifeBar1). TextBox::setCaption on Widget + once-only lost to Blood overwrite. Vanilla HUD only.
- v1.23 **rejected** (`46d810a`): Widget::setCaption every tick on LifeBar1. CoS / Dylan: Blood stays Blood. Do not ship that ready string.
- v1.23: LifeBar10 after Hunger. Grow MedicalPanel one row. Do not shift 1–9. Every-tick LifeBar10 write. Door clears LifeBar10 only. Ready: `ready v1.23 — LifeBar10 after Hunger, no LifeBar1 overwrite`.
