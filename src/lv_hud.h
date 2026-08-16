#pragma once

#include "lv_types.h"

// No TitleScreen hook. No title MyGUI.
// After In-game: left-stack Hemolymph/Vigor bar under the typical Blood slot.
// I-key tooltip is the backup (lv_hooks g_hudHave / hook_tip1).
void LvHudInstall(); // log only — do not create widgets
void LvHudEnsureAfterInGame(); // create the Blood HUD bar; no Character
void LvHudPaint(const CharSnap* snap);
void LvHudHide();
void LvHudNote(const CharSnap* snap);
