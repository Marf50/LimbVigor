# Limb Vigor

A [RE_Kenshi](https://www.nexusmods.com/kenshi/mods/847) plugin. Organic characters can grow a lost limb back. The resource sits on the **selected-character HUD, under Blood**.

Growth is not a silent number. Each stage is a real body part — same kind of item as a robot limb — with its own name, stats, and I-key slot tooltip. Stump → budding → forming → knitting → **grown**. Grown *is* the limb. The plugin does not rip it off to try original flesh.

This is not a feast-from-hunger hack. Three official KenshiLib hooks, the same numbers as the field-manual bench.

| Race | Resource | When it grows | Open air |
| --- | --- | --- | --- |
| Hive | Hemolymph | Always, if fed and not bleeding out | ~2.5 days |
| Shek | Battle-heat | Toughness 20, **or** a used Splint Kit | ~4 days |
| Human | Vigor | Toughness 40 **and** Field Medic 25, **or** a used Splint Kit | ~6 days |
| Skeleton | — | Never | — |

A bed roughly halves the time. One stump at a time, legs first. Open **I** and look at the limb slot: you should see `LV Budding Left Leg` (and so on) with worse athletics / dexterity that improve as it knits. A real prosthetic occupies the socket and blocks growth; progress is kept.

v1.7: stages are LimbReplacement parts (stats + I-key), slotted the same way robot limbs are. At 100% a **Grown** part stays in the socket. HUD bar under Blood (not the C sheet). Hover for the race rule and time. Dark UI ok.

## Get the DLL

GitHub Actions compiles `LimbVigor.dll` when I trigger it (not on every push). The last good zip is on [Releases](https://github.com/Marf50/LimbVigor/releases).

1. Open [Actions](https://github.com/Marf50/LimbVigor/actions)
2. Latest green **Build LimbVigor.dll** run
3. Download the **LimbVigor-mod** artifact
4. Extract the `LimbVigor` folder into `Kenshi/mods/` so `LimbVigor.mod` sits next to `LimbVigor.dll`
5. Enable **LimbVigor** on Kenshi’s Mods tab (it will not appear without the `.mod` file)

See [kenshi_mod/INSTALL.txt](kenshi_mod/INSTALL.txt).

## What is hooked

Only documented KenshiLib methods. No inventory windows, no MyGUI, no save-load hook.

| Hook | Why |
| --- | --- |
| `MedicalSystem::medicalUpdate` | Tick vigor and growth |
| `MedicalSystem::getMedicalGUIData` | Bar next to Blood |
| `MedicalSystem::applyDoctoring` | Splint Kit / stimulant starts the catalyst |

Addresses come from `KenshiLib::GetRealAddress`. The RVAs printed in the official headers are a fallback only.

Limb restore slots a growth part via `RobotLimbs::setLimb(REPLACED, item)` and `MedicalSystem::setRobotLimbItem`, created from LimbVigor.mod GameData (Economy limb as visual fallback). At 100% a Grown part stays — we do not call `setLimb(ORIGINAL)`. Progress is a sidecar file (`LimbVigor.progress`) next to the DLL — we do not hook `MedicalSystem::load`.

## Layout

```
src/LimbVigor.cpp     startPlugin
src/lv_sim.cpp        the numbers (no game types)
src/lv_game.cpp       race, limbs, HUD strings
src/lv_hooks.cpp      the three hooks
src/lv_config.cpp     LimbVigor.cfg / config.ini
src/lv_persist.cpp    LimbVigor.progress
src/lv_parts.cpp       growth-stage LimbReplacement catalog + equip
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

KenshiLib is GPLv3, so this plugin is GPLv3. Kenshi itself is Lo-Fi Games’.
