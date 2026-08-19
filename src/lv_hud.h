#pragma once

#include "lv_types.h"

// I-key tooltip snapshot only. No TitleScreen. No widget create.
// Layout: v1.26 LifeBar1–9 (unstretched 9-bar skin) + HemolymphStrip.
// After orig: ISub on Datapanel, numeric Value, pixel-width Green fill.
void LvHudInstall();
void LvHudEnsureAfterInGame();
void LvHudPaint(const CharSnap* snap);
void LvHudHide();
void LvHudNote(const CharSnap* snap);
