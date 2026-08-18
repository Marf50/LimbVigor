#pragma once

#include "lv_types.h"

// I-key tooltip snapshot only. No TitleScreen. No widget create.
// No layout file. Visible path is name-gated setCaption on LifeBar1 only.
void LvHudInstall();
void LvHudEnsureAfterInGame();
void LvHudPaint(const CharSnap* snap);
void LvHudHide();
void LvHudNote(const CharSnap* snap);
