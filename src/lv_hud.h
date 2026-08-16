#pragma once

#include "lv_types.h"

// No TitleScreen hook. No MyGUI widgets of any kind.
// Visible path is setLineProgress on medicalPanel (lv_game / hook_medGui).
// I-key tooltip (g_hudHave / hook_tip1) is the backup.
void LvHudInstall();              // log only — do not create widgets
void LvHudEnsureAfterInGame();    // no-op — do not create widgets
void LvHudPaint(const CharSnap* snap);
void LvHudHide();
void LvHudNote(const CharSnap* snap); // snapshot no-op
