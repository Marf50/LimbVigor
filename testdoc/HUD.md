# HUD notes (living)

Proven facts. Update when a playtest proves a new one.

## Layout names (Dark UI workshop 1200632417 + vanilla)

- Blood is **LifeBar1** (first bar). LifeBar2..9 = Head / body / limbs / Hunger.
- Dark UI has **no LifeBar10**. MyGUI `LifeBar10 not found` is Dark UI, not a missing Blood row.
- Caption **"Blood"** or **"Oil"** on a widget is extra confirmation only. It is **not** a write dest by itself.
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
- v1.21: `setCaption` on a dest chosen by Blood/Oil caption rank hid the **whole Kenshi HUD** (not just medical). Hypothesis: write hit a parent (Root / MainBar main widget / MedicalPanel) with empty `getName` or a Blood-looking caption that outranked LifeBar1. Treat as a playtest fact: parent write is fatal. Do not write unless `getName` contains `LifeBar1`.
- v1.22: **name-gated write.** `setCaption` only if live `getName` contains `LifeBar1`. Blood/Oil caption is confirmation, never a substitute. Empty name, Root, MedicalPanel, MedicalPanel_Back, StatusPanel → do not write (`painted=0`). Never `setVisible(false)` on a parent. Restore Blood/Oil only on that same gated widget.
- **No `setSize`.** Do not resize MedicalPanel / MedicalPanel_Back. Do not shift LifeBar2–9. No Dark UI layout fight.

## Landmines

No `createWidget`. No MainBar ctor `0x72C1E0`. No `eventFrameStart`. No TitleScreen MyGUI. No `ForgottenGUI::changeFontSize`. No DatapanelGUI `_NV_update` / `_NV_setObject`. No `GetRealAddress` on virtuals. No Gui tree walk. No Goal/State paint. No WindowCX. No `setSize` / MedicalPanel resize / LifeBar2–9 move. No `setVisible(false)` on a parent. No `setCaption` unless `getName` contains `LifeBar1`.

## Version trail

- v1.16: first `getInstancePtr` was ClipboardManager (`Gui` matches `MyGUI`). Export log cap 250. Treated DatapanelGUI* as Widget*.
- v1.17: Gui Singleton bind OK. Layout found (vanilla + Dark UI). Tree walk SEH'd.
- v1.18: `findWidgetT` bound; every unprefixed name null.
- v1.19: `_getWidget` 0x723780. Person-select crash. Prefix@+0x40 garbage. Retry after probe SEH killed the process.
- v1.20: person-select SEH-safe. Prove MainBar (`proven=1`). Skipped `_getWidget` (findWidgetT bound). Unprefixed lookup all null. `painted=0`. Crash lock PASS. CoS rejected dump-only.
- v1.21: hunt + `setCaption` on Blood-caption dest. No crash. **Parent write hid the whole HUD.**
- v1.22: name-gated `setCaption` LifeBar1 only. No parent write. No `setVisible`. `painted=0` if the name is missing or not LifeBar1.
