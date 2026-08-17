# HUD notes (living)

Proven facts. Update when a playtest proves a new one.

## Layout names (Dark UI workshop 1200632417 + vanilla)

- Blood is **LifeBar1** (first bar). LifeBar2..9 = Head / body / limbs / Hunger.
- Dark UI has **no LifeBar10**. MyGUI `LifeBar10 not found` is Dark UI, not a missing Blood row.
- Caption **"Blood"** or **"Oil"** on a widget is the LifeBar1 label.
- LifeBar1Value = Blood number. LifeBar1Green = Blood fill. Do not overwrite either.
- Captions (Blood/Head/…) are written by `MedicalSystem::getMedicalGUIData`.

## Pointers

- MainBarGUI* = ForgottenGUI `gui+0x10`. Prove with `*(bar+0x188) == medicalPanel`. That prove is a **CHECK** on the existing bar, not `medicalPanel - 0x188`.
- `+0x188` holds a **pointer to** MedicalDatapanel. Do **not** compute `MainBar = medicalPanel - 0x188`.
- medicalPanel is **MedicalDatapanel***, not DatapanelGUI. Do not `setLineProgress` on it.
- Goal/State is a different DatapanelGUI. Do not paint there.

## Find path (v1.21)

- Blood is a **MyGUI widget**. HUD write is `Widget::setCaption`, not Datapanel/`setLineProgress`.
- Scan proven MainBar member pointers whose vtable is in the **MyGUIEngine module range** (`GetModuleHandle` + `SizeOfImage`). Do not hardcode the v1.20 ASLR address (`0x6FFFFB960000`).
- BaseLayout prefix is a real MSVC `std::string` (size, cap, SSO vs heap). Never a C string at +0x40. v1.19 dumped `'0,000,000,048,8D2,510_'`.
- `_getWidget` RVA `0x723780`: once on MainBar this, once on `bar+0x30` (BaseLayout this). Hypothesis: v1.19 died because this was MainBar, not BaseLayout. SEH + C++ catch. **No retry.**
- 3-arg `findWidgetT(name, prefix, throw=false)` only if the prefix string is real. Unprefixed `findWidgetT` is null (v1.18).
- v1.20 crash lock PASS (`proven=1`) then refused `_getWidget` because findWidgetT was bound — all null, `painted=0`. Do **not** skip `_getWidget` on a proven bar.
- If any LifeBar1* or Blood/Oil-caption widget is found, **write** Hemolymph / Vigor / Battle-heat via `setCaption` in the same build. Person selected only. Restore Blood/Oil on door / box / chair.
- **No `setSize`.** Do not resize MedicalPanel / MedicalPanel_Back. Do not shift LifeBar2–9. First win is the Blood-row caption. A later cut may grow the panel. No Dark UI layout fight in v1.21.

## Landmines

No `createWidget`. No MainBar ctor `0x72C1E0`. No `eventFrameStart`. No TitleScreen MyGUI. No `ForgottenGUI::changeFontSize`. No DatapanelGUI `_NV_update` / `_NV_setObject`. No `GetRealAddress` on virtuals. No Gui tree walk. No Goal/State paint. No WindowCX. No `setSize` / MedicalPanel resize / LifeBar2–9 move.

## Version trail

- v1.16: first `getInstancePtr` was ClipboardManager (`Gui` matches `MyGUI`). Export log cap 250. Treated DatapanelGUI* as Widget*.
- v1.17: Gui Singleton bind OK. Layout found (vanilla + Dark UI). Tree walk SEH'd.
- v1.18: `findWidgetT` bound; every unprefixed name null.
- v1.19: `_getWidget` 0x723780. Person-select crash. Prefix@+0x40 garbage. Retry after probe SEH killed the process.
- v1.20: person-select SEH-safe. Prove MainBar (`proven=1`). Skipped `_getWidget` (findWidgetT bound). Unprefixed lookup all null. `painted=0`. Crash lock PASS. CoS rejected dump-only.
- v1.21: hunt MainBar MyGUI members + `_getWidget` once/once + 3-arg find if prefix real. `setCaption` Hemolymph on LifeBar1 / Blood if found. No Datapanel. No setSize.
