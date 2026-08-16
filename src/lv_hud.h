#pragma once

#include "lv_types.h"

// No TitleScreen hook. No widget create of any kind.
// Visible path: RE_Kenshi walk to live Blood, write Hemolymph/Vigor
// into an unused caption already on that HUD (setCaption/setSize only).
// I-key tooltip is the backup. _NV_say is separate (lv_game).
void LvHudInstall();              // log only — do not create widgets
void LvHudEnsureAfterInGame();    // UI-thread find/paint — do not create
void LvHudPaint(const CharSnap* snap);
void LvHudHide();
void LvHudNote(const CharSnap* snap);
