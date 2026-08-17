# HUD notes (living)

Proven facts. Update when a playtest proves a new one.

## Layout names (Dark UI workshop 1200632417 + vanilla)

- Blood is **LifeBar1** (first bar). LifeBar2..9 = Head / body / limbs / Hunger.
- Dark UI has **no LifeBar10**. MyGUI `LifeBar10 not found` is Dark UI, not a missing Blood row.
- LifeBar1Datapanel = label strip on the Blood bar (write target if `setLineProgress` works).
- LifeBar1Value = Blood number TextBox. LifeBar1Green = Blood fill. Do not overwrite either.
- Captions (Blood/Head/…) are written by `MedicalSystem::getMedicalGUIData`.

## Pointers

- MainBarGUI* = ForgottenGUI `gui+0x10`. Prove with `*(bar+0x188) == medicalPanel` (getMedicalPanel / known MedicalDatapanel*). That prove is a **CHECK** on the existing bar, not `medicalPanel - 0x188`.
- `+0x188` holds a **pointer to** MedicalDatapanel. Do **not** compute `MainBar = medicalPanel - 0x188`.
- medicalPanel is **MedicalDatapanel***, not DatapanelGUI. `getNumLines=0` there is a type mismatch. Do not `setLineProgress` on it.
- Goal/State is a different DatapanelGUI. Do not paint there.

## Find path

- `Gui::findWidgetT("LifeBar1")` is null. wraps::BaseLayout prefixes runtime names.
- MainBar+0x40 is a **GameStr / std::string**, not a C string. v1.19 dumped garbage `'0,000,000,048,8D2,510_'`. Do not treat +0x40 as a C string.
- `_getWidget` RVA `0x723780` (v1.19) crashed on **person-select**. Door/box/chair/cage never hit it. After `GUI probe SEH` the paint path **retried** `_getWidget` and the process died.
- v1.20: SEH every GUI call. On SEH log once, **do not retry `_getWidget`**, skip paint, growth continues. 3-arg `findWidgetT(name, prefix, throw=false)` is backup only. Paint on LifeBar1Datapanel is the win; not required to pass the person-select crash lock.

## Landmines

No `createWidget`. No MainBar ctor `0x72C1E0`. No `eventFrameStart`. No TitleScreen MyGUI. No `ForgottenGUI::changeFontSize`. No DatapanelGUI `_NV_update` / `_NV_setObject`. No `GetRealAddress` on virtuals. No Gui tree walk. No Goal/State paint. No WindowCX.

## Version trail

- v1.16: first `getInstancePtr` was ClipboardManager (`Gui` matches `MyGUI`). Export log cap 250. Treated DatapanelGUI* as Widget*.
- v1.17: Gui Singleton bind OK. Layout found (vanilla + Dark UI). Tree walk SEH'd.
- v1.18: `findWidgetT` bound; every unprefixed name null.
- v1.19: `_getWidget` 0x723780. Person-select crash. Prefix@+0x40 garbage. Retry after probe SEH killed the process.
- v1.20: person-select SEH-safe. Prove MainBar. No `_getWidget` retry. Skip paint on fail.
