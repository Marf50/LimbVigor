# HUD notes (living)

Proven facts. Update when a playtest proves a new one.

## Layout names (Dark UI workshop 1200632417 + LimbVigor override)

- Blood is **LifeBar1** (first bar). LifeBar2..9 = Head / Stomach / Chest / arms / legs / Hunger.
- Dark UI omitted **LifeBar10**. MyGUI `LifeBar10 not found` is Dark UI omitting a slot MainBar already `assignWidget`-binds.
- LimbVigor ships `kenshi_mod/gui/layout/Kenshi_MainPanel.layout` started from workshop **1200632417** Dark UI (MedicalPanel `position_real` locked). **Do not grow MedicalPanel.** Grow **MedicalPanel_Back only**, rescale LifeBar1–9 (`new_frac = old_frac * oldH/newH`) so pixel size stays the same, insert **LifeBar10 after Hunger** at the same pixel h as LifeBar9. Touch no other chrome.
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

## Write path (v1.24 — show LifeBar10 + Widget::setCaption)

- v1.22 find+write logged `painted=1` but Dylan saw only the vanilla HUD. Two causes:
  1. Bound `?setCaption@TextBox@MyGUI@@UEAAXAEBVUString@2@@Z` and called it on LifeBar1 (Widget). Wrong vtable. No pixels.
  2. Paint ran once. `getMedicalGUIData` writes Blood back every tick after that.
- CoS rejects another LifeBar1 overwrite. New row only: **LifeBar10 after Hunger.**
- Bind `Widget::setCaption` — prefer exact `?setCaption@Widget@MyGUI@@QEAAXAEBVUString@2@@Z`, keep UEAAX. Call that on LifeBar10 and LifeBar10Datapanel.
- `TextBox::setCaption` only on LifeBar10Value (the digit). Never TextBox-on-Widget. Never `setCaption` on LifeBar1.
- v1.23 playtest (Dylan): layout loaded (first real HUD change) but bars overflowed. Find succeeded: LifeBar10 `vis=0`, LifeBar10Value `vis=0`, LifeBar10Datapanel `vis=1`, orig on LifeBar10 was already `Blood`. `setCapW=0`. `TextBox::setCaption` on LifeBar10Value did not stick (`getCaption` empty).
- Bind `Widget::setCaption` for real — exact `?setCaption@Widget@MyGUI@@QEAAXAEBVUString@2@@Z` and UEAAX. Log `setCapW`. Retry if missing (do not latch a failed dump). LifeBar10 / LifeBar10Datapanel **must** use Widget::setCaption, not TextBox.
- `setVisible(true)` on **LifeBar10 and LifeBar10Value only** is OK this cut. Bind `Widget::setVisible`. Never `setVisible` on Root, MedicalPanel, MedicalPanel_Back, MedicalPanel_Front, LifeBar1–9, or any parent.
- Write **after orig** every selected-person tick. Read back `getCaption`. `painted=1` only if it is Hemolymph / Vigor / Battle-heat. Still Blood / empty is a fail.
- Door / box / chair: `setVisible(false)` or clear caption on LifeBar10 / LifeBar10Value only. Never touch LifeBar1. Blood stays Blood.
- No MedicalPanel grow. No full-layout drift. No LifeBar1 `setCaption`.

## Landmines

No `createWidget`. No MainBar ctor `0x72C1E0`. No `eventFrameStart`. No TitleScreen MyGUI. No `ForgottenGUI::changeFontSize`. No DatapanelGUI `_NV_update` / `_NV_setObject`. No `GetRealAddress` on virtuals. No Gui tree walk. No Goal/State paint. No WindowCX. No `setSize`. No `_getWidget`. No `setVisible` on Root / parents. No Datapanel / `setLineProgress` HUD path.

## Version trail

- v1.16–v1.18: bind / tree walk / unprefixed find all null.
- v1.19: `_getWidget` person-select crash. Retry after probe SEH killed the process.
- v1.20: crash lock PASS. Skipped `_getWidget`. Unprefixed find all null. `painted=0`.
- v1.21: prefix dumped as "ugly" and findWidgetT skipped. Then `_getWidget`/probe SEH. `painted=0`. **Whole HUD hid.** Prefix was real.
- v1.22: prefixed find+write succeeded (`painted=1` Hemolymph on LifeBar1). TextBox::setCaption on Widget + once-only lost to Blood overwrite. Vanilla HUD only.
- v1.23 **rejected** (`46d810a`): Widget::setCaption every tick on LifeBar1. CoS / Dylan: Blood stays Blood. Do not ship that ready string.
- v1.23 (`2bc122a`): LifeBar10 after Hunger, but grew MedicalPanel and drifted the whole layout. Children are `position_real` so parent scale changes pixel size. Bars larger, Blood off the top, Hunger off the bottom, name plate / chrome shifted. Find: LifeBar10 `vis=0`, `setCapW=0`, TextBox write empty.
- v1.24: Dark UI exact (MedicalPanel unchanged). Grow Back only, rescale 1–9. Show LifeBar10 / LifeBar10Value. Bind Widget::setCaption. Ready: `ready v1.24 — Dark UI exact + LifeBar10 tucked, no parent scale`.
