# Limb Vigor

A [RE_Kenshi](https://www.nexusmods.com/kenshi/mods/847) plugin. Organic characters can grow a lost limb back. The resource sits on the **medical panel, under Blood**.

This is not a feast-from-hunger hack and not a MyGUI overlay. Three official KenshiLib hooks, the same numbers as the field-manual bench.

| Race | Resource | When it grows | Open air |
| --- | --- | --- | --- |
| Hive | Hemolymph | Always, if fed and not bleeding out | ~2.5 days |
| Shek | Battle-heat | Toughness 20, **or** a used Splint Kit | ~4 days |
| Human | Vigor | Toughness 40 **and** Field Medic 25, **or** a used Splint Kit | ~6 days |
| Skeleton | — | Never | — |

A bed roughly halves the time. One stump at a time, legs first. The new limb is original flesh at ~22% of that part’s max HP. A prosthetic occupies the socket and blocks growth; progress is kept.

v1.2 does not talk to town NPCs and does not call character speech (that crashed World’s End on load). The medical bar is off until `EnableHud=1` after a clean load.

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

Limb restore uses `RobotLimbs::setLimb(ORIGINAL)` and `MedicalSystem::setRobotLimbItem`. Progress is a sidecar file (`LimbVigor.progress`) next to the DLL — we do not hook `MedicalSystem::load`.

## Layout

```
src/LimbVigor.cpp     startPlugin
src/lv_sim.cpp        the numbers (no game types)
src/lv_game.cpp       race, limbs, HUD strings
src/lv_hooks.cpp      the three hooks
src/lv_config.cpp     LimbVigor.cfg / config.ini
src/lv_persist.cpp    LimbVigor.progress
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
