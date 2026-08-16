# Limb Vigor

A [RE_Kenshi](https://www.nexusmods.com/kenshi/mods/847) plugin. Organic characters can grow a lost limb back.

Growth is not a silent number. Each stage is a real body part — same kind of item as a robot limb — with its own name, stats, and I-key slot tooltip. Stump → budding → forming → knitting → **grown**. Grown *is* the limb. The plugin does not rip it off to try original flesh.

Live numbers are written into an **unused caption already on the Blood HUD** (RE_Kenshi walk, name after the first `_`). `setCaption` / `setSize` only — never a sibling create (v1.8.1 found Blood, created children, died). Spoken lines use `_NV_say`. I-key on the LV part is the backup. C is skills — not the HUD. Door or building selected: hide the unused caption. If the walk finds no unused widget, `gui/LimbVigor.layout` is shipped for the game loader (`Kenshi_FloatingPanelSkin` / `Kenshi_TextboxStandardText`); C++ still only findWidget + setCaption. Does not replace Dark UI MainPanel.

This is not a feast-from-hunger hack. Medical tick, I-key tooltip, same numbers as the field-manual bench.

| Race | Resource | When it grows | Open air |
| --- | --- | --- | --- |
| Hive | Hemolymph | Always, if fed and not bleeding out | ~2.5 days |
| Shek | Battle-heat | Toughness 20, **or** a used Splint Kit | ~4 days |
| Human | Vigor | Toughness 40 **and** Field Medic 25, **or** a used Splint Kit | ~6 days |
| Skeleton | — | Never | — |

A bed roughly halves the time. One stump at a time, legs first. Open **I** and look at the limb slot: you should see `LV Budding Left Leg` (and so on) with worse athletics / dexterity that improve as it knits. A real prosthetic occupies the socket and blocks growth; progress is kept.

v1.9.9: after orig `getMedicalGUIData` / `_NV_getGUIData`, `setLineProgress` runs only if the `DatapanelGUI*` is the left medical panel (`ForgottenGUI* gui` → `mainbar` @ 0x10 → `getMedicalPanel()` RVA `0x71FDA0` / `medicalPanel` @ 0x188). If that `MedicalDatapanel*` is not the same pointer, we do not cast it — we only write the hook’s `DatapanelGUI*` when it has Blood. Never C / skills. `_NV_say` unchanged. No widget create.

The 1.9.1 playable-loop fixes stay: no Character until the world is in-game; I-key snap as soon as a player character exists; no Economy fallback; mesh-less GameData is refused; FileValue mesh/icon on every LV part. Grown stays — we do not call `setLimb(ORIGINAL)`.

## Get the DLL

GitHub Actions compiles `LimbVigor.dll` when I trigger it (not on every push). The last good zip is on [Releases](https://github.com/Marf50/LimbVigor/releases).

1. Open [Actions](https://github.com/Marf50/LimbVigor/actions)
2. Latest green **Build LimbVigor.dll** run
3. Download the **LimbVigor-mod** artifact
4. Extract the `LimbVigor` folder into `Kenshi/mods/` so `LimbVigor.mod` sits next to `LimbVigor.dll`
5. Enable **LimbVigor** on Kenshi’s Mods tab (it will not appear without the `.mod` file)

See [kenshi_mod/INSTALL.txt](kenshi_mod/INSTALL.txt).

## What is hooked

Documented KenshiLib methods. No TitleScreen hook. No widget create.

| Hook | Why |
| --- | --- |
| `MedicalSystem::medicalUpdate` | Tick vigor and growth; slot LV parts after in-game (snapshot only, no MyGUI) |
| `MedicalSystem::getMedicalGUIData` | After orig: I-key snapshot; `setLineProgress` on this `DatapanelGUI*` only if left medical (`gui` → `mainbar` @ 0x10 → `getMedicalPanel` / `medicalPanel` @ 0x188, or Blood on this panel) + character selected + in-game |
| `Character::_NV_getGUIData` | Same after orig. `GetRealAddress` on `_NV_getGUIData` only — never virtual `getGUIData` (RVA `0x5D3AE0`) |
| `MainBarGUI::_CONSTRUCTOR` | Stash MainBarGUI instance (RVA `0x72C1E0`); UI-thread find/paint leftover |
| `ForgottenGUI::changeFontSize` | UI-thread find/paint leftover (RVA `0x3E7C80`) — settings-button moment, no create |
| `MedicalSystem::applyDoctoring` | Splint Kit / stimulant starts the catalyst |
| `InventoryItemBase::getTooltipData1` | Live Hemolymph / stage / time on the I-key LV part |

Addresses come from `KenshiLib::GetRealAddress`. The RVAs printed in the official headers are a fallback only.

Limb restore slots a growth part via `RobotLimbs::setLimb(REPLACED, item)` and `MedicalSystem::setRobotLimbItem`, created from LimbVigor.mod GameData. That GameData must have FileValue mesh/icon — a mesh-less hit is skipped and we never createItem an Economy prosthetic. At 100% a Grown part stays — we do not call `setLimb(ORIGINAL)`. Progress is a sidecar file (`LimbVigor.progress`) next to the DLL — we do not hook `MedicalSystem::load`.

## Layout

```
src/LimbVigor.cpp     startPlugin
src/lv_sim.cpp        the numbers (no game types)
src/lv_game.cpp       race, limbs, STATS strings
src/lv_hooks.cpp      medical + I-key tooltip hooks
src/lv_config.cpp     LimbVigor.cfg / config.ini
src/lv_persist.cpp    LimbVigor.progress
src/lv_parts.cpp      growth-stage LimbReplacement catalog + equip
src/lv_hud.cpp        Blood walk + unused caption (no widget create)
kenshi_mod/gui/       LimbVigor.layout (game loader fallback)
src/lv_sim_test.cpp   headless checks (also run in CI)
```

Edit `kenshi_mod/config.ini` to retune. Restart the game. Do not hot-reload.

## Build it yourself

Windows, VS 2022, [KenshiLib_Examples_deps](https://github.com/BFrizzleFoShizzle/KenshiLib_Examples_deps) (git lfs pull):

```bat
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 ^
    -DKENSHILIB_DIR=...\KenshiLib ^
    -DBOOST_INCLUDE_DIR=...\boost_1_60_0 ^
    -DLIMBVIGOR_FORCE_GAME_BUILD=ON
cmake --build build --config Release
```

Linux (math only, not a game DLL):

```sh
cmake -S . -B build-test
cmake --build build-test --target lv_sim_test
./build-test/lv_sim_test
```
