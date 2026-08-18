# HUD notes (living)

Proven facts. Update when a playtest proves a new one.

## Layout names (Dark UI workshop 1200632417 + vanilla)

- Blood is **LifeBar1** (first bar). LifeBar2..9 = Head / body / limbs / Hunger.
- Dark UI has **no LifeBar10**. MyGUI `LifeBar10 not found` is Dark UI, not a missing Blood row.
- LifeBar1Value / LifeBar1Green / fill skins: log only. Do not setCaption them.
- Captions (Blood/Head/…) are written by `MedicalSystem::getMedicalGUIData`.

## Pointers

- MainBarGUI* = ForgottenGUI `gui+0x10`. Prove with `*(bar+0x188) == medicalPanel`. CHECK, not `medicalPanel - 0x188`.
- Root is at **MainBar+0x8**.
- `+0x188` is a pointer **to** MedicalDatapanel. Not DatapanelGUI. Do not `setLineProgress` on it.

## Prefix is real (v1.21)

- BaseLayout prefix @ +0x40 is a real MSVC `std::string` (v1.21: size=22 cap=31 heap=1).
- Value `'0,000,000,048,7F5,0E0_'` with MainBar `0x487F50B0`. That is **hex(bar+0x30) grouped in threes + `_`**. `0x487F50E0 == bar+0x30`.
- v1.19 `'0,000,000,048,8D2,510_'` is the same pattern. Not C-string garbage.
- getName on Root was `'0,000,000,048,7F5,0E0_Root'` — prefix + widget name.
- **Do not skip 3-arg findWidgetT because the prefix looks ugly.** That skip was the v1.21 miss.

## Find path (v1.22)

- 3-arg `findWidgetT(name, prefix, throw=false)` for `LifeBar1`, `LifeBar1Datapanel`, `LifeBar1Value`.
- Also `findWidgetT(prefix+name, throw=false)`.
- `Widget::findWidget` on Root (`bar+0x8`) for the short name and the full prefixed name.
- **Do not call `_getWidget` RVA `0x723780`.** v1.19 died on it. v1.21: scan logged 17 chrome members (no LifeBar), then `GUI probe SEH`, `painted=0`, whole HUD hid. `_getWidget` / the probe after the scan damaged MyGUI.
- MainBar member scan does not contain LifeBar / MedicalPanel. Log only. Not write dests.

## Write gate

- `setCaption` only if `getName` **ends with** `LifeBar1` or `LifeBar1Datapanel`, **or** `getCaption` is Blood/Oil.
- Never write Root, MedicalPanel, MedicalPanel_Back, StatusPanel, SquadPanel, Squad, Floor, Day, Money, Time, Biome, Paused, Loading, or fill bars.
- Never `setVisible`. No `setSize`. Door restores Blood/Oil on the same gated widget.

## Landmines

No `createWidget`. No MainBar ctor `0x72C1E0`. No `eventFrameStart`. No TitleScreen MyGUI. No `ForgottenGUI::changeFontSize`. No DatapanelGUI `_NV_update` / `_NV_setObject`. No `GetRealAddress` on virtuals. No Gui tree walk. No Goal/State paint. No WindowCX. No `setSize`. No `_getWidget`. No `setVisible(false)`.

## Version trail

- v1.16–v1.18: bind / tree walk / unprefixed find all null.
- v1.19: `_getWidget` person-select crash. Retry after probe SEH killed the process.
- v1.20: crash lock PASS. Skipped `_getWidget`. Unprefixed find all null. `painted=0`.
- v1.21: prefix dumped as "ugly" and findWidgetT skipped. Then `_getWidget`/probe SEH. `painted=0`. **Whole HUD hid.** Prefix was real.
- v1.22: use the real prefix with findWidgetT + Widget::findWidget on Root. No `_getWidget`. setCaption if gated dest found.
